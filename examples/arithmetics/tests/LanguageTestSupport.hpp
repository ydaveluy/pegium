#pragma once

#include <arithmetics/ast.hpp>

#include <pegium/core/parser/PegiumParser.hpp>

#include <sstream>
#include <string>
#include <vector>

namespace arithmetics::tests {

inline std::string dump_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    std::ostringstream current;
    current << diagnostic.kind;
    if (diagnostic.element != nullptr) {
      current << ":" << *diagnostic.element;
    }
    if (!diagnostic.message.empty()) {
      current << ":" << diagnostic.message;
    }
    current << "@" << diagnostic.beginOffset << "-" << diagnostic.endOffset;
    dump += current.str();
  }
  return dump;
}

inline std::string diagnostic_element_text(
    const pegium::parser::ParseDiagnostic &diagnostic) {
  if (diagnostic.element == nullptr) {
    return {};
  }
  std::ostringstream out;
  out << *diagnostic.element;
  return out.str();
}

inline std::string summarize_module_statements(const ast::Module &module) {
  std::string summary;
  for (const auto &statement : module.statements) {
    if (!summary.empty()) {
      summary += " | ";
    }
    if (const auto *definition =
            dynamic_cast<const ast::Definition *>(statement.get());
        definition != nullptr) {
      summary += "def:";
      summary += definition->name;
      continue;
    }
    if (dynamic_cast<const ast::Evaluation *>(statement.get()) != nullptr) {
      summary += "eval";
      continue;
    }
    summary += "other";
  }
  return summary;
}

inline std::string summarize_expression(const ast::Expression *expression) {
  if (expression == nullptr) {
    return "<null-expr>";
  }
  if (const auto *number = dynamic_cast<const ast::NumberLiteral *>(expression);
      number != nullptr) {
    return "number:" + std::to_string(number->value);
  }
  if (const auto *call = dynamic_cast<const ast::FunctionCall *>(expression);
      call != nullptr) {
    return "call:" + call->func.getRefText();
  }
  if (const auto *grouped =
          dynamic_cast<const ast::GroupedExpression *>(expression);
      grouped != nullptr) {
    return "group(" + summarize_expression(grouped->expression.get()) + ")";
  }
  if (const auto *binary =
          dynamic_cast<const ast::BinaryExpression *>(expression);
      binary != nullptr) {
    return "binary(" + summarize_expression(binary->left.get()) + " " +
           binary->op + " " + summarize_expression(binary->right.get()) + ")";
  }
  return "other-expr";
}

inline std::string summarize_module_statement_shapes(const ast::Module &module) {
  std::string summary;
  for (const auto &statement : module.statements) {
    if (!summary.empty()) {
      summary += " | ";
    }
    if (const auto *definition =
            dynamic_cast<const ast::Definition *>(statement.get());
        definition != nullptr) {
      summary += "def:";
      summary += definition->name;
      continue;
    }
    if (const auto *evaluation =
            dynamic_cast<const ast::Evaluation *>(statement.get());
        evaluation != nullptr) {
      summary += "eval:";
      summary += summarize_expression(evaluation->expression.get());
      continue;
    }
    summary += "other";
  }
  return summary;
}

} // namespace arithmetics::tests
