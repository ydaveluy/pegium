#include "BenchmarkSupport.hpp"

#include <arithmetics/services/Module.hpp>

#include <chrono>
#include <stdexcept>
#include <string>
#include <string_view>

#include <pegium/core/syntax-tree/AbstractReference.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>

namespace pegium::bench {
namespace {

using Clock = std::chrono::steady_clock;

volatile std::size_t g_scope_bench_sink = 0;

struct ScopeBenchSource {
  std::string text;
  std::string localReferenceName;
  std::string globalReferenceName;
};

ScopeBenchSource make_scope_source(std::size_t targetBytes) {
  std::string source;
  source.reserve(targetBytes + 4096);
  source += "Module scopeBench\n\n";

  std::size_t index = 0;
  while (source.size() < targetBytes / 2) {
    source += "def value" + std::to_string(index) + ": " +
              std::to_string(index % 97) + ";\n";
    ++index;
  }

  const auto parameterCount = std::max<std::size_t>(64, index / 2);
  const auto localName = "arg" + std::to_string(parameterCount - 1);
  const auto globalName = "value" + std::to_string(index - 1);
  source += "\ndef scoped(";
  for (std::size_t paramIndex = 0; paramIndex < parameterCount; ++paramIndex) {
    if (paramIndex > 0) {
      source += ", ";
    }
    source += "arg" + std::to_string(paramIndex);
  }
  source += "):\n    " + localName + " + " + globalName + ";\n";

  source += "\nscoped(";
  for (std::size_t argIndex = 0; argIndex < parameterCount; ++argIndex) {
    if (argIndex > 0) {
      source += ", ";
    }
    source += std::to_string(argIndex);
  }
  source += ");\n";
  return {.text = std::move(source),
          .localReferenceName = std::move(localName),
          .globalReferenceName = std::move(globalName)};
}

const AbstractReference &find_reference(const workspace::Document &document,
                                        std::string_view refText) {
  for (const auto &handle : document.references) {
    const auto &reference = *handle.getConst();
    if (reference.getRefText() == refText) {
      return reference;
    }
  }
  throw std::runtime_error("Could not find scope benchmark reference '" +
                           std::string(refText) + "'.");
}

std::shared_ptr<BenchmarkFixture> build_scope_fixture(std::string source) {
  ExampleBenchSpec spec{
      .name = "scope",
      .languageId = "arithmetics",
      .extension = ".calc",
      .registerLanguages = arithmetics::services::register_language_services,
      .makeSource = [](std::size_t) { return std::string{}; },
  };

  auto fixture = create_fixture(spec, std::move(source));
  pegium::services::installDefaultSharedCoreServices(*fixture->sharedServices);
  pegium::installDefaultSharedLspServices(*fixture->sharedServices);
  if (!spec.registerLanguages(*fixture->sharedServices)) {
    throw std::runtime_error("Failed to register language services for " +
                             spec.name + ".");
  }
  const auto *coreServices =
      &fixture->sharedServices->serviceRegistry->getServices(fixture->uri);
  fixture->services = pegium::as_services(coreServices);
  if (fixture->services == nullptr) {
    throw std::runtime_error("Registered services for '" + spec.languageId +
                             "' are not full services.");
  }
  create_changed_document(*fixture);

  workspace::BuildOptions options;
  options.validation = false;
  const std::array documents{fixture->document};
  fixture->sharedServices->workspace.documentBuilder->build(documents, options);
  if (fixture->document == nullptr ||
      fixture->document->state < workspace::DocumentState::Linked) {
    throw std::runtime_error("Scope benchmark document was not built.");
  }
  return fixture;
}

BenchmarkTimings measure_scope_operation(
    const std::shared_ptr<BenchmarkFixture> &fixture,
    const std::function<std::size_t()> &operation) {
  const auto start = Clock::now();
  g_scope_bench_sink += operation();
  const auto end = Clock::now();

  BenchmarkTimings timings{};
  timings[static_cast<std::size_t>(BenchmarkStep::FullBuild)] =
      std::chrono::duration<double, std::milli>(end - start).count();
  return timings;
}

} // namespace

void register_scope_benchmarks(BenchmarkRegistry &registry) {
  const auto source = make_scope_source(benchmark_target_bytes());
  const auto bytes = source.text.size();

  registry.add("scope-local", bytes, [source] {
    auto fixture = build_scope_fixture(source.text);
    const auto *scopeProvider = fixture->services->references.scopeProvider.get();
    if (fixture->document == nullptr) {
      throw std::runtime_error("Scope benchmark services are incomplete.");
    }

    const auto &localReference =
        find_reference(*fixture->document, source.localReferenceName);
    const auto localInfo = makeReferenceInfo(localReference);
    const auto iterations = get_env_int("PEGIUM_SCOPE_LOOKUP_ITERATIONS", 5000, 1);

    return measure_scope_operation(fixture, [&] {
      std::size_t total = 0;
      for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto *entry = scopeProvider->getScopeEntry(localInfo);
        if (entry != nullptr) {
          total += entry->name.size();
        }
      }
      return total;
    });
  });

  registry.add("scope-global", bytes, [source] {
    auto fixture = build_scope_fixture(source.text);
    const auto *scopeProvider = fixture->services->references.scopeProvider.get();
    if (fixture->document == nullptr) {
      throw std::runtime_error("Scope benchmark services are incomplete.");
    }

    const auto &globalReference =
        find_reference(*fixture->document, source.globalReferenceName);
    const auto globalInfo = makeReferenceInfo(globalReference);
    const auto iterations = get_env_int("PEGIUM_SCOPE_LOOKUP_ITERATIONS", 5000, 1);

    return measure_scope_operation(fixture, [&] {
      std::size_t total = 0;
      for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto *entry = scopeProvider->getScopeEntry(globalInfo);
        if (entry != nullptr) {
          total += entry->name.size();
        }
      }
      return total;
    });
  });

  registry.add("scope-all-elements", bytes, [source] {
    auto fixture = build_scope_fixture(source.text);
    const auto *scopeProvider = fixture->services->references.scopeProvider.get();
    if (fixture->document == nullptr) {
      throw std::runtime_error("Scope benchmark services are incomplete.");
    }

    const auto &globalReference =
        find_reference(*fixture->document, source.globalReferenceName);
    auto allInfo = makeReferenceInfo(globalReference);
    allInfo.referenceText = {};
    const auto iterations = get_env_int("PEGIUM_SCOPE_ENUM_ITERATIONS", 200, 1);

    return measure_scope_operation(fixture, [&] {
      std::size_t total = 0;
      for (int iteration = 0; iteration < iterations; ++iteration) {
        const auto completed = scopeProvider->visitScopeEntries(
            allInfo, [&total](const workspace::AstNodeDescription &entry) {
              total += entry.name.size();
              return true;
            });
        (void)completed;
      }
      return total;
    });
  });
}

} // namespace pegium::bench
