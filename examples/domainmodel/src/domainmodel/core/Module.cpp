#include <domainmodel/core/Module.hpp>

#include <utility>

#include <domainmodel/core/ModuleImpl.hpp>

namespace domainmodel {

void installDomainModelCoreModule(DomainModelCoreServices &services) {
  detail::applyDomainModelCoreModule(services);
}

std::unique_ptr<DomainModelCoreServices>
createDomainModelServices(const pegium::SharedCoreServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<DomainModelCoreServices>(
      sharedServices, std::move(languageId));
  installDomainModelCoreModule(*services);
  return services;
}

bool registerDomainModelServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel
