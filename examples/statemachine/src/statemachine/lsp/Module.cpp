#include <statemachine/lsp/Module.hpp>

#include <utility>

#include <statemachine/core/ModuleImpl.hpp>
#include <statemachine/lsp/StatemachineFormatter.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace statemachine {

void installStatemachineCoreModule(lsp::StatemachineServices &services) {
  detail::applyStatemachineCoreModule(services);
}

} // namespace statemachine

namespace statemachine::lsp {

void installStatemachineLspModule(StatemachineServices &services) {
  services.lsp.formatter = std::make_unique<StatemachineFormatter>(services);
}

std::unique_ptr<StatemachineServices>
createStatemachineServices(const pegium::SharedServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::makeDefaultServices<StatemachineServices>(
      sharedServices, std::move(languageId));
  statemachine::installStatemachineCoreModule(*services);
  installStatemachineLspModule(*services);
  return services;
}

bool registerStatemachineServices(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineServices(sharedServices));
  return true;
}

} // namespace statemachine::lsp
