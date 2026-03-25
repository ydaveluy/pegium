#pragma once

#include <memory>
#include <string>

#include <domainmodel/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace domainmodel::lsp {

/// Builds the LSP-enabled language services for the domainmodel example.
std::unique_ptr<DomainModelServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "domain-model");

/// Registers the LSP-enabled domainmodel services in `sharedServices`.
bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace domainmodel::lsp
