#pragma once

#include <statemachine/ast.hpp>

#include <pegium/services/Services.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace statemachine::services::validation {

class StatemachineValidator final {
public:
  static void registerValidationChecks(
      pegium::validation::ValidationRegistry &registry,
      const pegium::services::Services &services);

private:
  void checkStateNameStartsWithCapital(
      const statemachine::ast::State &state,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkUniqueNames(const statemachine::ast::Statemachine &model,
                        const pegium::validation::ValidationAcceptor &accept) const;
};

} // namespace statemachine::services::validation
