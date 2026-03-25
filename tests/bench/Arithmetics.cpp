#include "BenchmarkSupport.hpp"

#include <arithmetics/services/Module.hpp>

namespace pegium::bench {
namespace {

std::string make_source(std::size_t targetBytes) {
  std::string source;
  source.reserve(targetBytes + 1024);
  source += "module Bench\n\n";

  std::size_t index = 0;
  while (source.size() < targetBytes) {
    if (index == 0) {
      source += "def value0: 1 + 2;\n";
    } else {
      source += "def value" + std::to_string(index) + ": value" +
                std::to_string(index - 1) + " + 1;\n";
    }

    if (index % 16 == 0) {
      source += "value" + std::to_string(index) + ";\n";
    }
    ++index;
  }

  return source;
}

} // namespace

void register_arithmetics_benchmarks(BenchmarkRegistry &registry) {
  register_full_build_benchmark(
      registry,
      {.name = "arithmetics",
       .languageId = "arithmetics",
       .extension = ".calc",
       .registerLanguages = arithmetics::register_language_services,
       .makeSource = make_source});
}

} // namespace pegium::bench
