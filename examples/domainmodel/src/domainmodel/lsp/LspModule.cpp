#include <domainmodel/lsp/LspModule.hpp>

#include <utility>

#include <domainmodel/core/CoreModule.hpp>
#include <domainmodel/lsp/DomainModelFormatter.hpp>
#include <domainmodel/lsp/DomainModelRenameProvider.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace domainmodel {

void installDomainModelLspModule(DomainModelServices &services) {
  services.lsp.renameProvider =
      std::make_unique<DomainModelRenameProvider>(services);
  services.lsp.formatter = std::make_unique<DomainModelFormatter>(services);
}

std::unique_ptr<DomainModelServices>
createDomainModelLspServices(const pegium::SharedServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  // The container is-a CoreServices and is-a DomainModelAddedServices, so it
  // binds both parameters of the (non-template) core wiring.
  installDomainModelCoreModule(*services, *services);
  installDomainModelLspModule(*services);
  return services;
}

bool registerDomainModelLspServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelLspServices(sharedServices));
  return true;
}

} // namespace domainmodel
