#pragma once

#include <memory>
#include <string>
#include <utility>

#include <statemachine/core/Services.hpp>
#include <statemachine/parser/Parser.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>

namespace statemachine {

/// Core service overrides applied on top of pegium's default core services.
template <typename Services>
void installStatemachineCoreModule(Services &services) {
  services.parser =
      std::make_unique<const parser::StateMachineParser>(services);
  services.languageMetaData.fileExtensions = {".statemachine"};
  services.validator = std::make_unique<validation::StatemachineValidator>();
  validation::registerValidationChecks(services);
}

/// Builds the core-only statemachine language services.
inline std::unique_ptr<StatemachineCoreServices>
createStatemachineServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId = "statemachine") {
  auto services = std::make_unique<StatemachineCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installStatemachineCoreModule(*services);
  return services;
}

/// Registers the core-only statemachine services in `sharedServices`.
inline bool
registerStatemachineServices(pegium::SharedCoreServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      createStatemachineServices(sharedServices));
  return true;
}

} // namespace statemachine
