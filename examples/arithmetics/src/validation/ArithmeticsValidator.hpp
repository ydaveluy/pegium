#pragma once

#include <arithmetics/ast.hpp>
#include <arithmetics/services/Services.hpp>

#include <string_view>

#include <pegium/core/validation/ValidationAcceptor.hpp>

namespace arithmetics::services::validation {

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

void registerValidationChecks(
    arithmetics::services::ArithmeticsServices &services);

} // namespace arithmetics::services::validation
