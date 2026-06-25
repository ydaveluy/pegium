#include <domainmodel/lsp/Module.hpp>

#include <utility>

#include <domainmodel/core/ModuleImpl.hpp>
#include <domainmodel/lsp/DomainModelFormatter.hpp>
#include <domainmodel/lsp/DomainModelRenameProvider.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace domainmodel {

void installDomainModelCoreModule(lsp::DomainModelServices &services) {
  detail::applyDomainModelCoreModule(services);
}

} // namespace domainmodel

namespace domainmodel::lsp {

void installDomainModelLspModule(DomainModelServices &services) {
  services.lsp.renameProvider =
      std::make_unique<DomainModelRenameProvider>(services);
  services.lsp.formatter = std::make_unique<DomainModelFormatter>(services);
}

std::unique_ptr<DomainModelServices>
createDomainModelServices(const pegium::SharedServices &sharedServices,
                          std::string languageId) {
  auto services = pegium::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  domainmodel::installDomainModelCoreModule(*services);
  installDomainModelLspModule(*services);
  return services;
}

bool registerDomainModelServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel::lsp
