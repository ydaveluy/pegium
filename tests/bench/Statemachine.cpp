#include "BenchmarkSupport.hpp"

#include <statemachine/services/Module.hpp>

namespace pegium::bench {
namespace {

std::string make_source(std::size_t targetBytes) {
  constexpr std::size_t kEventCount = 64;
  constexpr std::size_t kCommandCount = 64;

  std::string source;
  source.reserve(targetBytes + 1024);
  source += "statemachine Bench\n";
  source += "events";
  for (std::size_t index = 0; index < kEventCount; ++index) {
    source += " Event" + std::to_string(index);
  }
  source += "\ncommands";
  for (std::size_t index = 0; index < kCommandCount; ++index) {
    source += " Command" + std::to_string(index);
  }
  source += "\ninitialState State0\n";

  std::size_t stateIndex = 0;
  while (source.size() < targetBytes) {
    source += "state State" + std::to_string(stateIndex) + " actions { Command" +
              std::to_string(stateIndex % kCommandCount) + " }\n";
    source += "Event" + std::to_string(stateIndex % kEventCount) + " => State" +
              std::to_string(stateIndex + 1) + "\n";
    source += "end\n";
    ++stateIndex;
  }

  source += "state State" + std::to_string(stateIndex) + " actions { Command0 }\n";
  source += "Event0 => State0\n";
  source += "end\n";
  return source;
}

} // namespace

void register_statemachine_benchmarks(BenchmarkRegistry &registry) {
  register_full_build_benchmark(
      registry,
      {.name = "statemachine",
       .languageId = "statemachine",
       .extension = ".statemachine",
       .registerLanguages = statemachine::register_language_services,
       .makeSource = make_source});
}

} // namespace pegium::bench
