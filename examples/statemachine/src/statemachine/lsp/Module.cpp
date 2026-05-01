#include <statemachine/lsp/Module.hpp>

#include <utility>

#include <statemachine/core/validation/StatemachineValidator.hpp>
#include <statemachine/lsp/StatemachineFormatter.hpp>
#include <statemachine/parser/Parser.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace statemachine {

namespace {
template <typename Services>
void applyStatemachineCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::StateMachineParser>(services);
  services.languageMetaData.fileExtensions = {".statemachine"};
  services.validator = std::make_unique<validation::StatemachineValidator>();
  validation::registerValidationChecks(services);
}
} // namespace

void installStatemachineCoreModule(lsp::StatemachineServices &services) {
  applyStatemachineCoreModule(services);
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
