#pragma once

#include <arithmetics/ast.hpp>

#include <pegium/services/Services.hpp>
#include <pegium/validation/ValidationRegistry.hpp>

namespace arithmetics::services::validation {

class ArithmeticsValidator final {
public:
  static void registerValidationChecks(
      pegium::validation::ValidationRegistry &registry,
      const pegium::services::Services &services);

private:
  void checkDivisionByZero(
      const arithmetics::ast::BinaryExpression &expression,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkDefinition(const arithmetics::ast::Definition &definition,
                       const pegium::validation::ValidationAcceptor &accept) const;
  void checkFunctionCall(const arithmetics::ast::FunctionCall &call,
                         const pegium::validation::ValidationAcceptor &accept) const;
  void checkModule(const arithmetics::ast::Module &module,
                   const pegium::validation::ValidationAcceptor &accept) const;
};

} // namespace arithmetics::services::validation
