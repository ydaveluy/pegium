#pragma once

#include <memory>
#include <string>
#include <utility>

#include <domainmodel/core/Module.hpp>
#include <domainmodel/lsp/DomainModelFormatter.hpp>
#include <domainmodel/lsp/DomainModelRenameProvider.hpp>
#include <domainmodel/lsp/Services.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace domainmodel::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
inline void installDomainModelLspModule(DomainModelServices &services) {
  services.lsp.renameProvider =
      std::make_unique<DomainModelRenameProvider>(services);
  services.lsp.formatter = std::make_unique<DomainModelFormatter>(services);
}

/// Builds the LSP-enabled domain-model language services.
inline std::unique_ptr<DomainModelServices>
createDomainModelServices(const pegium::SharedServices &sharedServices,
                          std::string languageId = "domain-model") {
  auto services = pegium::makeDefaultServices<DomainModelServices>(
      sharedServices, std::move(languageId));
  installDomainModelCoreModule(*services);
  installDomainModelLspModule(*services);
  return services;
}

/// Registers the LSP-enabled domain-model services in `sharedServices`.
inline bool
registerDomainModelServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createDomainModelServices(sharedServices));
  return true;
}

} // namespace domainmodel::lsp
