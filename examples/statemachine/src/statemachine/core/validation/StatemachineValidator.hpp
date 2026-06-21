#pragma once

#include <statemachine/core/ast.hpp>

#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace statemachine::validation {

class StatemachineValidator final {
public:
  void checkStateNameStartsWithCapital(
      const statemachine::ast::State &state,
      const pegium::validation::ValidationAcceptor &accept) const;

  void checkUniqueStatesAndEvents(
      const statemachine::ast::Statemachine &model,
      const pegium::validation::ValidationAcceptor &accept) const;
};

template <typename TServices>
void registerValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkStateNameStartsWithCapital>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &StatemachineValidator::checkUniqueStatesAndEvents>(validator)});
}

} // namespace statemachine::validation
