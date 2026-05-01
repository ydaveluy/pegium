#pragma once

#include <memory>
#include <string>

#include <statemachine/lsp/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace statemachine {

/// Core service overrides applied on top of pegium's default core services,
/// overload targeting the LSP-enabled service bundle.
void installStatemachineCoreModule(statemachine::lsp::StatemachineServices &services);

} // namespace statemachine

namespace statemachine::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
void installStatemachineLspModule(StatemachineServices &services);

/// Builds the LSP-enabled statemachine language services.
std::unique_ptr<StatemachineServices>
createStatemachineServices(const pegium::SharedServices &sharedServices,
                           std::string languageId = "statemachine");

/// Registers the LSP-enabled statemachine services in `sharedServices`.
bool registerStatemachineServices(pegium::SharedServices &sharedServices);

} // namespace statemachine::lsp
