#include "BenchmarkSupport.hpp"

#include <requirements/services/Module.hpp>

namespace pegium::bench {
namespace {

std::string make_source(std::size_t targetBytes) {
  constexpr std::size_t kEnvironmentCount = 24;

  std::string source;
  source.reserve(targetBytes + 1024);
  source += "contact: \"bench\"\n";
  for (std::size_t index = 0; index < kEnvironmentCount; ++index) {
    source += "environment Env" + std::to_string(index) + ": \"Environment " +
              std::to_string(index) + "\"\n";
  }

  std::size_t requirementIndex = 0;
  while (source.size() < targetBytes) {
    source += "req REQ" + std::to_string(requirementIndex) +
              " \"Requirement " + std::to_string(requirementIndex) +
              "\" applicable for Env" +
              std::to_string(requirementIndex % kEnvironmentCount) + ", Env" +
              std::to_string((requirementIndex + 1) % kEnvironmentCount) + "\n";
    ++requirementIndex;
  }

  return source;
}

} // namespace

void register_requirements_benchmarks(BenchmarkRegistry &registry) {
  register_full_build_benchmark(
      registry,
      {.name = "requirements",
       .languageId = "requirements-lang",
       .extension = ".req",
       .registerLanguages = requirements::register_language_services,
       .makeSource = make_source});
}

} // namespace pegium::bench
