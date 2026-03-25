#pragma once

#include <arithmetics/ast.hpp>
#include <arithmetics/services/Services.hpp>

#include <string_view>

#include <pegium/core/validation/ValidationAcceptor.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>

namespace arithmetics::validation {

namespace IssueCodes {

inline constexpr std::string_view ExpressionNormalizable =
    "arithmetics.expression-normalizable";

} // namespace IssueCodes

class ArithmeticsValidator final {
public:
  void checkDivByZero(
      const arithmetics::ast::BinaryExpression &expression,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkUniqueParameters(
      const arithmetics::ast::Definition &definition,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkNormalizable(
      const arithmetics::ast::Definition &definition,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkUniqueDefinitions(
      const arithmetics::ast::Module &module,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkFunctionRecursion(
      const arithmetics::ast::Module &module,
      const pegium::validation::ValidationAcceptor &accept) const;
  void checkMatchingParameters(
      const arithmetics::ast::FunctionCall &call,
      const pegium::validation::ValidationAcceptor &accept) const;
};

template <typename TServices>
void registerValidationChecks(TServices &services) {
  auto &registry = *services.validation.validationRegistry;
  auto &validator = *services.arithmetics.validation.arithmeticsValidator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkDivByZero>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkUniqueParameters>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkNormalizable>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkUniqueDefinitions>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkFunctionRecursion>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkMatchingParameters>(validator)});
}

} // namespace arithmetics::validation
