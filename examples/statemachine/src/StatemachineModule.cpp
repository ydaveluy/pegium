#include <statemachine/services/Module.hpp>

#include <statemachine/parser/Parser.hpp>

#include <memory>
#include <string>
#include <utility>

#include "lsp/StatemachineFormatter.hpp"
#include "validation/StatemachineValidator.hpp"

#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace statemachine::services {

StatemachineServices::StatemachineServices(
    const pegium::SharedServices &sharedServices)
    : pegium::Services(sharedServices) {}

StatemachineServices::StatemachineServices(StatemachineServices &&) noexcept = default;

StatemachineServices::~StatemachineServices() noexcept = default;

std::unique_ptr<StatemachineServices>
create_language_services(const pegium::SharedServices &sharedServices,
                         std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices<StatemachineServices>(
          sharedServices, std::move(languageId));
  services->parser =
      std::make_unique<const statemachine::parser::StateMachineParser>(*services);
  services->languageMetaData.fileExtensions = {".statemachine"};
  services->statemachine.validation.statemachineValidator =
      std::make_unique<validation::StatemachineValidator>();
  services->lsp.formatter = std::make_unique<lsp::StatemachineFormatter>(*services);
  validation::registerValidationChecks(*services);
  return services;
}

bool register_language_services(pegium::SharedServices &sharedServices) {
  sharedServices.serviceRegistry->registerServices(
      create_language_services(sharedServices, "statemachine"));
  return true;
}

} // namespace statemachine::services
