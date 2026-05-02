#pragma once

#include <gtest/gtest.h>
#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pegium::test::recovery {

struct RecoveryNode : pegium::AstNode {
  string token;
};

struct RecoveryStatement : pegium::AstNode {};

struct RecoveryDefinition : RecoveryStatement {
  string name;
  int value = 0;
};

struct RecoveryExpression : pegium::AstNode {};

struct RecoveryNumberExpression : RecoveryExpression {
  int value = 0;
};

struct RecoveryReferenceExpression : RecoveryExpression {
  string name;
};

struct RecoveryFunctionCall : RecoveryExpression {
  string name;
  vector<pointer<RecoveryExpression>> args;
};

struct RecoveryDefinitionWithExpr : pegium::AstNode {
  string name;
  pointer<RecoveryExpression> expr;
};

struct RecoveryParameter : pegium::AstNode {
  string name;
};

struct RecoveryDefinitionWithOptionalArgs : pegium::AstNode {
  string name;
  vector<pointer<RecoveryParameter>> args;
  pointer<RecoveryExpression> expr;
};

struct RecoveryEvaluation : RecoveryStatement {
  string name;
};

struct RecoveryExpressionEvaluation : RecoveryStatement {
  pointer<RecoveryExpression> expression;
};

struct RecoveryBinaryExpression : RecoveryExpression {
  pointer<RecoveryExpression> left;
  string op;
  pointer<RecoveryExpression> right;
};

struct RecoveryGroupedExpression : RecoveryExpression {
  pointer<RecoveryExpression> expression;
};

struct RecoveryModule : pegium::AstNode {
  string name;
  vector<pointer<pegium::AstNode>> statements;
};

struct RecoveryContact : pegium::AstNode {
  string name;
};

struct RecoveryFeatureNode : pegium::AstNode {
  bool many = false;
  string name;
  string type;
};

struct RecoveryFeatureListNode : pegium::AstNode {
  vector<pointer<RecoveryFeatureNode>> features;
};

struct RecoveryEnvironmentNode : pegium::AstNode {
  string name;
  string label;
};

struct RecoveryRequirementNode : pegium::AstNode {
  string name;
  string label;
};

struct RecoveryRequirementModelNode : pegium::AstNode {
  pointer<RecoveryContact> contact;
  vector<pointer<RecoveryEnvironmentNode>> environments;
  vector<pointer<RecoveryRequirementNode>> requirements;
};

struct RecoveryNameListNode : pegium::AstNode {
  vector<string> names;
};

struct RecoveryPrefixedNameListNode : pegium::AstNode {
  string name;
  string label;
  vector<string> names;
};

struct RecoveryTaggedRequirementListNode : pegium::AstNode {
  string name;
  string label;
  vector<string> environments;
};

struct RecoveryTaggedRequirementModelNode : pegium::AstNode {
  vector<pointer<RecoveryEnvironmentNode>> environments;
  vector<pointer<RecoveryTaggedRequirementListNode>> requirements;
};

struct RecoveryTransitionBlockNode : pegium::AstNode {};

struct RecoveryTransitionNode : pegium::AstNode {
  string event;
  string target;
};

struct RecoveryStateNode : pegium::AstNode {
  string name;
  vector<pointer<RecoveryTransitionNode>> transitions;
};

struct RecoveryStateModelNode : pegium::AstNode {
  vector<pointer<RecoveryStateNode>> states;
};

template <typename T> struct ValueNode : pegium::AstNode {
  T value{};
};

struct ParsedResult {
  pegium::parser::ParseResult result;
  std::unique_ptr<pegium::RootCstNode> &cst;
  pegium::AstNode *&value;
  std::vector<pegium::parser::ParseDiagnostic> &parseDiagnostics;
  pegium::TextOffset &parsedLength;
  bool &fullMatch;

  explicit ParsedResult(pegium::parser::ParseResult parseResult)
      : result(std::move(parseResult)), cst(result.cst), value(result.value),
        parseDiagnostics(result.parseDiagnostics),
        parsedLength(result.parsedLength), fullMatch(result.fullMatch) {}
};

template <typename T>
inline ParsedResult
parseDataType(const pegium::parser::DataTypeRule<T> &rule,
              std::string_view text, const pegium::parser::Skipper &skipper,
              const pegium::parser::ParseOptions &options = {}) {
  pegium::parser::ParserRule<ValueNode<T>> root{
      "Root", pegium::parser::assign<&ValueNode<T>::value>(rule)};
  return ParsedResult{
      pegium::test::parse_rule_result(root, text, skipper, options)};
}

template <typename T>
inline ParsedResult
parseTerminal(const pegium::parser::TerminalRule<T> &rule,
              std::string_view text, const pegium::parser::Skipper &skipper,
              const pegium::parser::ParseOptions &options = {}) {
  pegium::parser::ParserRule<ValueNode<T>> root{
      "Root", pegium::parser::assign<&ValueNode<T>::value>(rule)};
  return ParsedResult{
      pegium::test::parse_rule_result(root, text, skipper, options)};
}

template <typename RuleType>
inline ParsedResult
parseRule(const RuleType &rule, std::string_view text,
          const pegium::parser::Skipper &skipper,
          const pegium::parser::ParseOptions &options = {}) {
  return ParsedResult{
      pegium::test::parse_rule_result(rule, text, skipper, options)};
}

inline std::string dump_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    dump += std::to_string(diagnostic.beginOffset);
    dump += "-";
    dump += std::to_string(diagnostic.endOffset);
    dump += ":";
    switch (diagnostic.kind) {
    case pegium::parser::ParseDiagnosticKind::Inserted:
      dump += "Inserted";
      break;
    case pegium::parser::ParseDiagnosticKind::Deleted:
      dump += "Deleted";
      break;
    case pegium::parser::ParseDiagnosticKind::Replaced:
      dump += "Replaced";
      break;
    case pegium::parser::ParseDiagnosticKind::Incomplete:
      dump += "Incomplete";
      break;
    case pegium::parser::ParseDiagnosticKind::Recovered:
      dump += "Recovered";
      break;
    case pegium::parser::ParseDiagnosticKind::ConversionError:
      dump += "ConversionError";
      break;
    }
    if (diagnostic.element != nullptr) {
      dump += ":";
      std::ostringstream oss;
      oss << *diagnostic.element;
      dump += oss.str();
    }
    if (!diagnostic.message.empty()) {
      dump += ":";
      dump += diagnostic.message;
    }
  }
  return dump;
}

} // namespace pegium::test::recovery
