#include <statemachine/lsp/LspModule.hpp>

#include <utility>

#include <statemachine/core/CoreModule.hpp>
#include <statemachine/lsp/StatemachineFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace statemachine {

void installStatemachineLspModule(StatemachineServices &services) {
  services.lsp.formatter = std::make_unique<StatemachineFormatter>(services);
}

std::unique_ptr<StatemachineServices>
createStatemachineLspServices(const pegium::SharedServices &sharedServices,
                              std::string languageId) {
  auto services = pegium::makeDefaultServices<StatemachineServices>(
      sharedServices, std::move(languageId));
  // The container is-a CoreServices and is-a StatemachineAddedServices, so it
  // binds both parameters of the (non-template) core wiring.
  installStatemachineCoreModule(*services, *services);
  installStatemachineLspModule(*services);
  return services;
}

bool registerStatemachineLspServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineLspServices(sharedServices));
  return true;
}

} // namespace statemachine
