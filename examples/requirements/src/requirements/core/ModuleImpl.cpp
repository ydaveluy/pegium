#include <requirements/core/ModuleImpl.hpp>

#include <requirements/core/RequirementsParser.hpp>

namespace requirements::parser {

std::unique_ptr<const pegium::parser::Parser>
makeRequirementsParser(const pegium::CoreServices &services) {
  return std::make_unique<const RequirementsParser>(services);
}

std::unique_ptr<const pegium::parser::Parser>
makeTestsParser(const pegium::CoreServices &services) {
  return std::make_unique<const TestsParser>(services);
}

} // namespace requirements::parser
