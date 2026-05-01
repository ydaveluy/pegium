#pragma once

#include <memory>
#include <string>

#include <domainmodel/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace domainmodel {

/// Core service overrides applied on top of pegium's default core services,
/// overload targeting the LSP-enabled service bundle.
void installDomainModelCoreModule(domainmodel::lsp::DomainModelServices &services);

} // namespace domainmodel

namespace domainmodel::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
void installDomainModelLspModule(DomainModelServices &services);

/// Builds the LSP-enabled domain-model language services.
std::unique_ptr<DomainModelServices>
createDomainModelServices(const pegium::SharedServices &sharedServices,
                          std::string languageId = "domain-model");

/// Registers the LSP-enabled domain-model services in `sharedServices`.
bool registerDomainModelServices(pegium::SharedServices &sharedServices);

} // namespace domainmodel::lsp
