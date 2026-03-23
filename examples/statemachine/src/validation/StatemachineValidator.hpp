#pragma once

#include <statemachine/ast.hpp>

#include <statemachine/services/Services.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace statemachine::services::validation {

class StatemachineValidator final {
public:
  void checkStateNameStartsWithCapital(
      const statemachine::ast::State &state,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkUniqueStatesAndEvents(
      const statemachine::ast::Statemachine &model,
      const pegium::validation::ValidationAcceptor &accept) const;
};

void registerValidationChecks(
    statemachine::services::StatemachineServices &services);

} // namespace statemachine::services::validation
