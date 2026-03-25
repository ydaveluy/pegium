#pragma once

#include <memory>
#include <string>

#include <statemachine/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace statemachine::lsp {

/// Builds the LSP-enabled language services for the statemachine example.
std::unique_ptr<StatemachineServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId = "statemachine");

/// Registers the LSP-enabled statemachine services in `sharedServices`.
bool register_language_services(pegium::SharedServices &sharedServices);

} // namespace statemachine::lsp
