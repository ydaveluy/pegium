#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <functional>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pegium/lsp/workspace/TextDocuments.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/Disposable.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/DocumentBuilder.hpp>

namespace pegium::bench {

inline int get_env_int(std::string_view name, int defaultValue,
                       int minValue = 0) {
  if (const auto *value = std::getenv(std::string(name).c_str())) {
    const auto parsed = std::atoi(value);
    return std::max(parsed, minValue);
  }
  return defaultValue;
}

inline std::size_t get_env_size(std::string_view name, std::size_t defaultValue,
                                std::size_t minValue = 0) {
  if (const auto *value = std::getenv(std::string(name).c_str())) {
    const auto parsed = std::strtoull(value, nullptr, 10);
    return std::max<std::size_t>(parsed, minValue);
  }
  return defaultValue;
}

inline std::string get_env_string(std::string_view name) {
  if (const auto *value = std::getenv(std::string(name).c_str())) {
    return value;
  }
  return {};
}

inline std::size_t benchmark_target_bytes() {
  return get_env_size("PEGIUM_BENCH_BYTES", 64 * 1024, 16 * 1024);
}

// The builder runs three merged phases, each publishing its constituent document
// states in one post-barrier burst. Timing the individual sub-states would report
// ~0 for every state but the last of each phase, so the benchmark reports the
// three real phases instead, each at its boundary state.
enum class BenchmarkStep : std::size_t {
  ParseIndex,  // phase A: parse + index exported content (-> IndexedContent)
  ScopeLink,   // phase B: local scopes + link + index references (-> IndexedReferences)
  Validation,  // phase C: validate (-> Validated)
  FullBuild,
  Count,
};

constexpr std::size_t benchmark_step_count() {
  return static_cast<std::size_t>(BenchmarkStep::Count);
}

inline std::string_view benchmark_step_name(BenchmarkStep step) {
  switch (step) {
  case BenchmarkStep::ParseIndex:
    return "parse+index";
  case BenchmarkStep::ScopeLink:
    return "scope+link";
  case BenchmarkStep::Validation:
    return "validate";
  case BenchmarkStep::FullBuild:
    return "full-build";
  case BenchmarkStep::Count:
    break;
  }
  return "unknown";
}

using BenchmarkTimings = std::array<double, benchmark_step_count()>;
using BenchmarkIteration = std::function<BenchmarkTimings()>;

struct BenchmarkCase {
  std::string name;
  std::size_t bytes = 0;
  BenchmarkIteration run;
  // Workspace benchmarks only report the full build (time + throughput).
  bool fullBuildOnly = false;
};

class BenchmarkRegistry {
public:
  void add(std::string name, std::size_t bytes, BenchmarkIteration run,
           bool fullBuildOnly = false) {
    _cases.push_back({.name = std::move(name),
                      .bytes = bytes,
                      .run = std::move(run),
                      .fullBuildOnly = fullBuildOnly});
  }

  int runAll(std::string_view filter = {}) const {
    const auto iterations = get_env_int("PEGIUM_BENCH_ITERATIONS", 3, 1);
    std::size_t executed = 0;

    for (const auto &benchCase : _cases) {
      if (!filter.empty() && benchCase.name.find(filter) == std::string::npos) {
        continue;
      }
      ++executed;

      // Progress trace on stderr (results go to stdout): a long benchmark would
      // otherwise look like the run had hung.
      std::cerr << "  running " << benchCase.name << " (" << iterations
                << " iter)\n";

      BenchmarkTimings totals{};
      for (int iterationIndex = 0; iterationIndex < iterations;
           ++iterationIndex) {
        const auto timings = benchCase.run();
        for (std::size_t step = 0; step < benchmark_step_count(); ++step) {
          totals[step] += timings[step];
        }
      }
      for (auto &value : totals) {
        value /= static_cast<double>(iterations);
      }

      std::cout << "[bench] " << benchCase.name << " size=" << benchCase.bytes
                << "B iterations=" << iterations << '\n';
      for (std::size_t step = 0; step < benchmark_step_count(); ++step) {
        if (benchCase.fullBuildOnly &&
            static_cast<BenchmarkStep>(step) != BenchmarkStep::FullBuild) {
          continue;
        }
        const auto averageMs = totals[step];
        const auto averageSeconds = averageMs / 1000.0;
        const auto mibPerSecond = averageSeconds > 0.0
                                      ? (static_cast<double>(benchCase.bytes) /
                                         static_cast<double>(1024 * 1024)) /
                                            averageSeconds
                                      : 0.0;
        std::cout << "  " << std::setw(18) << std::left
                  << benchmark_step_name(static_cast<BenchmarkStep>(step))
                  << std::fixed << std::setprecision(2) << std::setw(10)
                  << averageMs << "ms  " << std::setw(10) << mibPerSecond
                  << "MiB/s\n";
      }
    }

    if (executed == 0) {
      std::cerr << "No benchmark matched filter '" << filter << "'.\n";
      return 1;
    }
    return 0;
  }

private:
  std::vector<BenchmarkCase> _cases;
};

struct ExampleBenchSpec {
  std::string name;
  std::string languageId;
  std::string extension;
  bool (*registerLanguages)(pegium::SharedCoreServices &) = nullptr;
  std::function<std::string(std::size_t targetBytes)> makeSource;
};

struct BenchmarkFixture {
  std::shared_ptr<pegium::SharedServices> sharedServices;
  const pegium::CoreServices *services = nullptr;
  std::shared_ptr<workspace::Document> document;
  std::string uri;
  std::string languageId;
  std::string source;
};

inline std::shared_ptr<pegium::SharedServices> make_empty_shared_services() {
  return std::make_shared<pegium::SharedServices>();
}

inline std::shared_ptr<BenchmarkFixture>
create_fixture(const ExampleBenchSpec &spec, std::string source) {
  if (spec.registerLanguages == nullptr) {
    throw std::runtime_error("No language registration function provided.");
  }

  auto fixture = std::make_shared<BenchmarkFixture>();
  fixture->sharedServices = make_empty_shared_services();
  fixture->languageId = spec.languageId;
  fixture->uri = utils::path_to_file_uri("/tmp/pegium-bench/" + spec.name +
                                         spec.extension);
  fixture->source = std::move(source);
  return fixture;
}

inline std::shared_ptr<workspace::Document>
create_changed_document(BenchmarkFixture &fixture) {
  const auto documents = fixture.sharedServices->lsp.textDocuments;
  if (documents == nullptr) {
    throw std::runtime_error("Missing shared text document manager.");
  }

  auto textDocument = std::make_shared<workspace::TextDocument>(
      workspace::TextDocument::create(fixture.uri, fixture.languageId, 1,
                                      fixture.source));
  (void)documents->set(textDocument);
  const auto storedTextDocument = documents->get(fixture.uri);
  if (storedTextDocument == nullptr) {
    throw std::runtime_error("Failed to open benchmark text document.");
  }

  auto document =
      std::make_shared<workspace::Document>(storedTextDocument, fixture.uri);
  fixture.sharedServices->workspace.documents->addDocument(document);
  fixture.document = document;
  return document;
}

inline BenchmarkTimings
measure_full_build_iteration(const std::shared_ptr<BenchmarkFixture> &fixture) {
  // One boundary state per merged phase (each phase publishes its states in a
  // single post-barrier burst, so the last state of each is its end time):
  //   A parse + index content          -> IndexedContent
  //   B local scopes + link + index refs -> IndexedReferences
  //   C validate                       -> Validated
  constexpr std::array phaseStates{
      workspace::DocumentState::IndexedContent,
      workspace::DocumentState::IndexedReferences,
      workspace::DocumentState::Validated,
  };

  using Clock = std::chrono::steady_clock;
  std::array<std::optional<Clock::time_point>, phaseStates.size()> phaseTimes;
  utils::DisposableStore disposables;
  for (std::size_t index = 0; index < phaseStates.size(); ++index) {
    disposables.add(
        fixture->sharedServices->workspace.documentBuilder->onBuildPhase(
            phaseStates[index],
            [&phaseTimes,
             index](std::span<const std::shared_ptr<workspace::Document>>,
                    const utils::CancellationToken &) {
              phaseTimes[index] = Clock::now();
            }));
  }

  const auto start = Clock::now();
  const std::array documents{fixture->document};
  workspace::BuildOptions options;
  options.validation = true;
  fixture->sharedServices->workspace.documentBuilder->build(documents, options);
  const auto end = Clock::now();
  if (fixture->document == nullptr ||
      fixture->document->state != workspace::DocumentState::Validated) {
    throw std::runtime_error("Benchmark full build failed.");
  }

  for (std::size_t index = 0; index < phaseTimes.size(); ++index) {
    if (!phaseTimes[index].has_value()) {
      throw std::runtime_error(
          "Missing benchmark phase event for " +
          std::string(benchmark_step_name(static_cast<BenchmarkStep>(index))) +
          ".");
    }
  }

  const auto millis = [](Clock::time_point from, Clock::time_point to) {
    return std::chrono::duration<double, std::milli>(to - from).count();
  };

  BenchmarkTimings timings{};
  timings[static_cast<std::size_t>(BenchmarkStep::ParseIndex)] =
      millis(start, *phaseTimes[0]);
  timings[static_cast<std::size_t>(BenchmarkStep::ScopeLink)] =
      millis(*phaseTimes[0], *phaseTimes[1]);
  timings[static_cast<std::size_t>(BenchmarkStep::Validation)] =
      millis(*phaseTimes[1], *phaseTimes[2]);
  timings[static_cast<std::size_t>(BenchmarkStep::FullBuild)] =
      millis(start, end);
  return timings;
}

inline void register_full_build_benchmark(BenchmarkRegistry &registry,
                                          const ExampleBenchSpec &spec) {
  // Skip generating (and holding) inputs the active filter excludes, so a
  // single-config run's peak RSS reflects only that benchmark's input.
  const auto filter = get_env_string("PEGIUM_BENCH_FILTER");
  if (!filter.empty() && spec.name.find(filter) == std::string::npos) {
    return;
  }
  const auto source = spec.makeSource(benchmark_target_bytes());
  const auto bytes = source.size();

  registry.add(spec.name, bytes, [spec, source] {
    auto fixture = create_fixture(spec, source);
    pegium::installDefaultSharedCoreServices(*fixture->sharedServices);
    pegium::installDefaultSharedLspServices(*fixture->sharedServices);
    if (!spec.registerLanguages(*fixture->sharedServices)) {
      throw std::runtime_error("Failed to register language services for " +
                               spec.name + ".");
    }
    fixture->services =
        &fixture->sharedServices->serviceRegistry->getServices(fixture->uri);
    create_changed_document(*fixture);
    return measure_full_build_iteration(fixture);
  });
}

} // namespace pegium::bench
