#include <domainmodel/services/Module.hpp>

#include <memory>
#include <string>
#include <utility>

#include "DomainModelCoreSetup.hpp"

namespace domainmodel {

DomainModelServices::DomainModelServices(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::CoreServices(sharedServices) {}

DomainModelServices::DomainModelServices(DomainModelServices &&) noexcept = default;

DomainModelServices::~DomainModelServices() noexcept = default;

std::unique_ptr<DomainModelServices>
create_language_services(
    const pegium::SharedCoreServices &sharedServices,
                         std::string languageId) {
  auto services = std::make_unique<DomainModelServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  detail::configure_core_services(*services);
  return services;
}

bool register_language_services(
    pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "domain-model"));
  return true;
}

} // namespace domainmodel
