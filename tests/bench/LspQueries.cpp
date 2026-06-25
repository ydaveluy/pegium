#include "BenchmarkSupport.hpp"

#include <arithmetics/core/Module.hpp>
#include <arithmetics/core/ast.hpp>

#include <array>
#include <chrono>
#include <functional>
#include <stdexcept>
#include <string>

#include <pegium/core/references/References.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/lsp/symbols/WorkspaceSymbolProvider.hpp>

// Micro-bench for the COLD LSP-query paths the fresh-eyes audit flagged:
//  - IndexManager::allElements() copies the whole index per call   (finding #1)
//  - References::findReferences() / get_self_nodes on a high-fan-in symbol
//    scans every incoming reference                                (finding #4)
// The workspace is built ONCE per case (setup, untimed); only the query loops
// are timed. Reported in the "full-build" slot (ms); the MiB/s column is
// meaningless here — read the millisecond figure.
namespace pegium::bench {
namespace {

using Clock = std::chrono::steady_clock;
volatile std::size_t g_lspquery_sink = 0;

// One `base` definition referenced by `fanin` other definitions: `fanin+1`
// exported symbols (index size, finding #1) and `fanin` references to `base`
// (incoming fan-in, finding #4).
std::string make_lspquery_source(std::size_t fanin) {
  std::string source;
  source.reserve(fanin * 24 + 64);
  source += "Module lspBench\n";
  source += "def base: 1;\n";
  for (std::size_t index = 0; index < fanin; ++index) {
    source += "def use" + std::to_string(index) + ": base;\n";
  }
  return source;
}

std::shared_ptr<BenchmarkFixture> build_lspquery_fixture(std::string source) {
  ExampleBenchSpec spec{
      .name = "lspquery",
      .languageId = "arithmetics",
      .extension = ".calc",
      .registerLanguages = arithmetics::registerArithmeticsServices,
      .makeSource = [](std::size_t) { return std::string{}; },
  };

  auto fixture = create_fixture(spec, std::move(source));
  pegium::installDefaultSharedCoreServices(*fixture->sharedServices);
  pegium::installDefaultSharedLspServices(*fixture->sharedServices);
  if (!spec.registerLanguages(*fixture->sharedServices)) {
    throw std::runtime_error("Failed to register arithmetics services.");
  }
  fixture->services =
      &fixture->sharedServices->serviceRegistry->getServices(fixture->uri);
  create_changed_document(*fixture);

  workspace::BuildOptions options;
  options.validation = true; // full build -> references indexed
  const std::array documents{fixture->document};
  fixture->sharedServices->workspace.documentBuilder->build(documents, options);
  if (fixture->document == nullptr ||
      fixture->document->state != workspace::DocumentState::Validated) {
    throw std::runtime_error("LSP-query benchmark document was not built.");
  }
  return fixture;
}

const AstNode *find_base_definition(const workspace::Document &document) {
  const auto *module =
      dynamic_cast<const arithmetics::ast::Module *>(document.parseResult.value);
  if (module == nullptr) {
    throw std::runtime_error("LSP-query benchmark module missing.");
  }
  for (const auto *statement : module->statements) {
    if (const auto *definition =
            dynamic_cast<const arithmetics::ast::Definition *>(statement);
        definition != nullptr && definition->name == "base") {
      return definition;
    }
  }
  throw std::runtime_error("LSP-query benchmark 'base' definition missing.");
}

BenchmarkTimings measure(const std::function<std::size_t()> &operation) {
  const auto start = Clock::now();
  g_lspquery_sink += operation();
  const auto end = Clock::now();
  BenchmarkTimings timings{};
  timings[static_cast<std::size_t>(BenchmarkStep::FullBuild)] =
      std::chrono::duration<double, std::milli>(end - start).count();
  return timings;
}

} // namespace

void register_lspquery_benchmarks(BenchmarkRegistry &registry) {
  const auto fanin = get_env_size("PEGIUM_LSPQUERY_FANIN", 4000, 1);
  const auto source = make_lspquery_source(fanin);
  const auto bytes = source.size();
  const auto iterations = get_env_int("PEGIUM_LSPQUERY_ITERATIONS", 200, 1);

  // #1: IndexManager::allElements() — whole-index materialization per call.
  registry.add(
      "lspquery-allElements", bytes,
      [source, iterations] {
        auto fixture = build_lspquery_fixture(source);
        const auto &indexManager = *fixture->sharedServices->workspace.indexManager;
        return measure([&] {
          std::size_t total = 0;
          for (int i = 0; i < iterations; ++i) {
            total += indexManager.allElements().size();
          }
          return total;
        });
      },
      /*fullBuildOnly=*/true);

  // #1 end-to-end: workspaceSymbols query (allElements + fuzzy match).
  registry.add(
      "lspquery-workspaceSymbols", bytes,
      [source, iterations] {
        auto fixture = build_lspquery_fixture(source);
        const auto *provider =
            fixture->sharedServices->lsp.workspaceSymbolProvider.get();
        if (provider == nullptr) {
          throw std::runtime_error("Missing workspace symbol provider.");
        }
        ::lsp::WorkspaceSymbolParams params;
        params.query = "use";
        return measure([&] {
          std::size_t total = 0;
          for (int i = 0; i < iterations; ++i) {
            total += provider->getSymbols(params).size();
          }
          return total;
        });
      },
      /*fullBuildOnly=*/true);

  // #4: findReferences on a high-fan-in symbol (exercises get_self_nodes).
  registry.add(
      "lspquery-findReferences-highfanin", bytes,
      [source, iterations] {
        auto fixture = build_lspquery_fixture(source);
        const auto *references = fixture->services->references.references.get();
        const auto *baseNode = find_base_definition(*fixture->document);
        return measure([&] {
          std::size_t total = 0;
          for (int i = 0; i < iterations; ++i) {
            total += references
                         ->findReferences(*baseNode,
                                          {.includeDeclaration = true})
                         .size();
          }
          return total;
        });
      },
      /*fullBuildOnly=*/true);
}

} // namespace pegium::bench
