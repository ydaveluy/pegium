#include "validation/ArithmeticsValidator.hpp"

#include <arithmetics/ast.hpp>

#include <cmath>
#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/services/JsonValue.hpp>
#include <pegium/validation/DiagnosticRanges.hpp>

namespace arithmetics::services::validation {
namespace {

using namespace arithmetics::ast;
using pegium::services::DiagnosticSeverity;

const Definition *as_definition(const pegium::AstNode *node) {
  return dynamic_cast<const Definition *>(node);
}

std::optional<double> evaluate_constant_expression(const Expression &expression) {
  if (const auto *number = dynamic_cast<const NumberLiteral *>(&expression)) {
    return number->value;
  }

  if (const auto *grouped = dynamic_cast<const GroupedExpression *>(&expression)) {
    if (!grouped->expression) {
      return std::nullopt;
    }
    return evaluate_constant_expression(*grouped->expression);
  }

  const auto *binary = dynamic_cast<const BinaryExpression *>(&expression);
  if (binary == nullptr || !binary->left || !binary->right) {
    return std::nullopt;
  }

  const auto left = evaluate_constant_expression(*binary->left);
  const auto right = evaluate_constant_expression(*binary->right);
  if (!left.has_value() || !right.has_value()) {
    return std::nullopt;
  }

  if (binary->op == "+") {
    return *left + *right;
  }
  if (binary->op == "-") {
    return *left - *right;
  }
  if (binary->op == "*") {
    return *left * *right;
  }
  if (binary->op == "^") {
    return std::pow(*left, *right);
  }
  if (binary->op == "%") {
    if (*right == 0.0) {
      return std::nullopt;
    }
    return std::fmod(*left, *right);
  }
  if (binary->op == "/") {
    if (*right == 0.0) {
      return std::nullopt;
    }
    return *left / *right;
  }

  return std::nullopt;
}

void collect_function_calls(const Expression *expression,
                            std::vector<const FunctionCall *> &calls) {
  if (expression == nullptr) {
    return;
  }
  if (const auto *call = dynamic_cast<const FunctionCall *>(expression)) {
    calls.push_back(call);
    for (const auto &arg : call->args) {
      collect_function_calls(arg.get(), calls);
    }
    return;
  }
  if (const auto *binary = dynamic_cast<const BinaryExpression *>(expression)) {
    collect_function_calls(binary->left.get(), calls);
    collect_function_calls(binary->right.get(), calls);
    return;
  }
  if (const auto *grouped = dynamic_cast<const GroupedExpression *>(expression)) {
    collect_function_calls(grouped->expression.get(), calls);
  }
}

} // namespace

void ArithmeticsValidator::registerValidationChecks(
    pegium::validation::ValidationRegistry &registry,
    const pegium::services::Services & /*services*/) {
  const ArithmeticsValidator validator;
  registry.registerChecks(
      {pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkDivisionByZero>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkDefinition>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkFunctionCall>(validator),
       pegium::validation::ValidationRegistry::makeValidationCheck<
           &ArithmeticsValidator::checkModule>(validator)});
}

void ArithmeticsValidator::checkDivisionByZero(
    const BinaryExpression &expression,
    const pegium::validation::ValidationAcceptor &accept) const {
  if ((expression.op == "/" || expression.op == "%") && expression.right &&
      evaluate_constant_expression(*expression.right).value_or(1.0) == 0.0) {
    accept.error(expression, "Division by zero is detected.");
  }
}

void ArithmeticsValidator::checkDefinition(
    const Definition &definition,
    const pegium::validation::ValidationAcceptor &accept) const {
  std::unordered_map<std::string, std::size_t> params;
  for (const auto &argument : definition.args) {
    if (!argument) {
      continue;
    }
    const auto count = ++params[argument->name];
    if (count > 1) {
      accept.error(*argument, "Duplicate parameter name: " + argument->name)
          .property<&DeclaredParameter::name>();
    }
  }

  if (!definition.expr) {
    return;
  }
  if (dynamic_cast<const NumberLiteral *>(definition.expr.get()) != nullptr) {
    return;
  }
  const auto constant = evaluate_constant_expression(*definition.expr);
  if (!constant.has_value()) {
    return;
  }

  pegium::services::JsonValue::Object payload;
  payload["constant"] = pegium::services::JsonValue(*constant);
  accept
      .info(*definition.expr, std::format(
                                  "Expression could be normalized to constant {}",
                                  *constant))
      .code("arithmetics.expression-normalizable")
      .data(pegium::services::JsonValue(std::move(payload)));
}

void ArithmeticsValidator::checkFunctionCall(
    const FunctionCall &call,
    const pegium::validation::ValidationAcceptor &accept) const {
  const auto *definition = as_definition(call.func.get());
  if (definition == nullptr) {
    return;
  }
  if (definition->args.size() != call.args.size()) {
    accept.error(call, std::format("Function {} expects {} parameters, but {} "
                                   "were given.",
                                   definition->name, definition->args.size(),
                                   call.args.size()));
  }
}

void ArithmeticsValidator::checkModule(
    const Module &module,
    const pegium::validation::ValidationAcceptor &accept) const {
  std::unordered_map<std::string, std::vector<const Definition *>> definitions;
  std::unordered_map<std::string, std::vector<std::string>> graph;
  std::unordered_map<std::string, const Definition *> definitionByName;

  for (const auto &statement : module.statements) {
    const auto *definition = dynamic_cast<const Definition *>(statement.get());
    if (definition == nullptr) {
      continue;
    }

    definitions[definition->name].push_back(definition);
    definitionByName.emplace(definition->name, definition);

    std::vector<const FunctionCall *> calls;
    collect_function_calls(definition->expr.get(), calls);
    for (const auto *call : calls) {
      const auto *callee = as_definition(call->func.get());
      if (callee == nullptr) {
        continue;
      }
      graph[definition->name].push_back(callee->name);
    }
  }

  for (const auto &[name, nodes] : definitions) {
    if (nodes.size() <= 1) {
      continue;
    }
    for (const auto *definition : nodes) {
      accept.error(*definition, "Duplicate definition name: " + name)
          .property<&Definition::name>();
    }
  }

  enum class VisitState { Unvisited, Visiting, Visited };
  std::unordered_map<std::string, VisitState> states;
  bool hasCycle = false;
  const Definition *cycleDefinition = nullptr;

  std::function<void(const std::string &)> visit = [&](const std::string &name) {
    if (hasCycle) {
      return;
    }
    auto &state = states[name];
    if (state == VisitState::Visiting) {
      hasCycle = true;
      if (const auto it = definitionByName.find(name);
          it != definitionByName.end()) {
        cycleDefinition = it->second;
      }
      return;
    }
    if (state == VisitState::Visited) {
      return;
    }
    state = VisitState::Visiting;
    if (const auto it = graph.find(name); it != graph.end()) {
      for (const auto &callee : it->second) {
        visit(callee);
      }
    }
    state = VisitState::Visited;
  };

  for (const auto &entry : graph) {
    visit(entry.first);
  }

  if (hasCycle) {
    if (cycleDefinition != nullptr) {
      accept.error(*cycleDefinition, "Recursion is not allowed.")
          .property<&Definition::name>();
    } else {
      accept.error(module, "Recursion is not allowed.");
    }
  }
}

} // namespace arithmetics::services::validation
#include <format>
