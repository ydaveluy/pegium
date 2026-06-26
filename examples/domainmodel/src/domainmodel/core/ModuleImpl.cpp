#include <domainmodel/core/ModuleImpl.hpp>

#include <domainmodel/core/DomainModelParser.hpp>

namespace domainmodel::parser {

std::unique_ptr<const pegium::parser::Parser>
makeDomainModelParser(const pegium::CoreServices &services) {
  return std::make_unique<const DomainModelParser>(services);
}

} // namespace domainmodel::parser
