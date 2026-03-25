#pragma once

#include <memory>

#include <statemachine/parser/Parser.hpp>

#include "validation/StatemachineValidator.hpp"

namespace statemachine::detail {

template <typename TServices>
void configure_core_services(TServices &services) {
  services.parser =
      std::make_unique<const statemachine::parser::StateMachineParser>(services);
  services.languageMetaData.fileExtensions = {".statemachine"};
  services.statemachine.validation.statemachineValidator =
      std::make_unique<statemachine::validation::StatemachineValidator>();
  statemachine::validation::registerValidationChecks(services);
}

} // namespace statemachine::detail
