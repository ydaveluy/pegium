#include <statemachine/services/Module.hpp>

#include <statemachine/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/StatemachineFormatter.hpp"
#include "validation/StatemachineValidator.hpp"

namespace statemachine::services {

std::unique_ptr<pegium::services::Services>
create_language_services(const pegium::services::SharedServices &sharedServices,
                         std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const statemachine::parser::StateMachineParser>(*services);
  services->languageMetaData.fileExtensions = {".statemachine"};
  services->lsp.formatter = std::make_unique<lsp::StatemachineFormatter>(*services);
  validation::StatemachineValidator::registerValidationChecks(
      *services->validation.validationRegistry, *services);
  return services;
}

bool register_language_services(pegium::services::SharedServices &sharedServices) {
  return sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "statemachine"));
}

} // namespace statemachine::services
