#include "BenchmarkSupport.hpp"

#include <domainmodel/services/Module.hpp>

namespace pegium::bench {
namespace {

std::string make_source(std::size_t targetBytes) {
  std::string source;
  source.reserve(targetBytes + 1024);
  source += "datatype String\n";
  source += "package bench {\n";

  std::size_t index = 0;
  while (source.size() < targetBytes) {
    source += "  entity Entity" + std::to_string(index);
    if (index > 0) {
      source += " extends Entity" + std::to_string(index - 1);
    }
    source += " {\n";
    source += "    name: String\n";
    if (index > 0) {
      source += "    prev: Entity" + std::to_string(index - 1) + "\n";
    }
    source += "  }\n";
    ++index;
  }

  source += "}\n";
  return source;
}

std::string make_polymorphic_source(std::size_t targetBytes) {
  std::string source;
  source.reserve(targetBytes + 1024);
  source += "datatype String\n";
  source += "datatype Number\n";
  source += "package bench {\n";

  std::size_t index = 0;
  while (source.size() < targetBytes) {
    source += "  entity Entity" + std::to_string(index) + " {\n";
    source += "    label: String\n";
    source += "    count: Number\n";
    if (index > 0) {
      source += "    previous: Entity" + std::to_string(index - 1) + "\n";
    }
    source += "  }\n";
    ++index;
  }

  source += "}\n";
  return source;
}

} // namespace

void register_domainmodel_benchmarks(BenchmarkRegistry &registry) {
  register_full_build_benchmark(
      registry,
      {.name = "domainmodel",
       .languageId = "domain-model",
       .extension = ".dmodel",
       .registerLanguages = domainmodel::register_language_services,
       .makeSource = make_source});
  register_full_build_benchmark(
      registry,
      {.name = "domainmodel-polymorphic-links",
       .languageId = "domain-model",
       .extension = ".dmodel",
       .registerLanguages = domainmodel::register_language_services,
       .makeSource = make_polymorphic_source});
}

} // namespace pegium::bench
