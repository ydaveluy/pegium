#include <domainmodel/parser/Parser.hpp>

#include <utility>

namespace domainmodel::parser {

std::unique_ptr<pegium::services::Services>
make_language_services(const pegium::services::SharedServices &sharedServices,
                       std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::make_unique<const DomainModelParser>(*services);
  return services;
}

} // namespace domainmodel::parser
