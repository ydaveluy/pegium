#include <statemachine/core/Module.hpp>

#include <utility>

#include <statemachine/core/validation/StatemachineValidator.hpp>
#include <statemachine/parser/Parser.hpp>

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

void installStatemachineCoreModule(StatemachineCoreServices &services) {
  applyStatemachineCoreModule(services);
}

std::unique_ptr<StatemachineCoreServices>
createStatemachineServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = std::make_unique<StatemachineCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installStatemachineCoreModule(*services);
  return services;
}

bool registerStatemachineServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineServices(sharedServices));
  return true;
}

} // namespace statemachine
