#pragma once

#include <memory>
#include <string>
#include <utility>

#include <statemachine/core/Module.hpp>
#include <statemachine/lsp/Services.hpp>
#include <statemachine/lsp/StatemachineFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace statemachine::lsp {

/// LSP service overrides applied on top of pegium's default LSP services.
inline void installStatemachineLspModule(StatemachineServices &services) {
  services.lsp.formatter = std::make_unique<StatemachineFormatter>(services);
}

/// Builds the LSP-enabled statemachine language services.
inline std::unique_ptr<StatemachineServices>
createStatemachineServices(const pegium::SharedServices &sharedServices,
                           std::string languageId = "statemachine") {
  auto services = pegium::makeDefaultServices<StatemachineServices>(
      sharedServices, std::move(languageId));
  installStatemachineCoreModule(*services);
  installStatemachineLspModule(*services);
  return services;
}

/// Registers the LSP-enabled statemachine services in `sharedServices`.
inline bool
registerStatemachineServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineServices(sharedServices));
  return true;
}

} // namespace statemachine::lsp
