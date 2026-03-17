#include <pegium/converter/CstJsonConverter.hpp>

#include <cassert>
#include <sstream>

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Create.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/Nest.hpp>
#include <pegium/grammar/OrderedChoice.hpp>

namespace pegium::converter {
namespace {

std::string assignment_operator_source(grammar::AssignmentOperator op) {
  switch (op) {
  case grammar::AssignmentOperator::Assign:
    return "=";
  case grammar::AssignmentOperator::Append:
    return "+=";
  case grammar::AssignmentOperator::EnableIf:
    return "?=";
  }
  return "=";
}

std::string element_source(const grammar::AbstractElement &element);

std::string ordered_choice_source(const grammar::OrderedChoice &choice) {
  std::string source = "(";
  for (std::size_t index = 0; index < choice.size(); ++index) {
    if (index > 0) {
      source += '|';
    }
    source += element_source(*choice.get(index));
  }
  source += ')';
  return source;
}

std::string element_source(const grammar::AbstractElement &element) {
  switch (element.getKind()) {
  case grammar::ElementKind::DataTypeRule:
  case grammar::ElementKind::ParserRule:
  case grammar::ElementKind::TerminalRule:
    return std::string(
        static_cast<const grammar::AbstractRule &>(element).getName());
  case grammar::ElementKind::Literal: {
    std::ostringstream stream;
    stream << static_cast<const grammar::Literal &>(element);
    return stream.str();
  }
  case grammar::ElementKind::OrderedChoice:
    return ordered_choice_source(
        static_cast<const grammar::OrderedChoice &>(element));
  case grammar::ElementKind::Create:
    return std::string(static_cast<const grammar::Create &>(element).getTypeName());
  default: {
    std::ostringstream stream;
    stream << element;
    return stream.str();
  }
  }
}

std::string assignment_source(const grammar::Assignment &assignment) {
  return std::string(assignment.getFeature()) +
         assignment_operator_source(assignment.getOperator()) +
         element_source(*assignment.getElement());
}

std::string grammar_source(const grammar::AbstractElement *grammarElement) {
  assert(grammarElement && "Every CstNode must reference a grammar element");
  switch (grammarElement->getKind()) {
  case grammar::ElementKind::Create: {
    const auto &create = static_cast<const grammar::Create &>(*grammarElement);
    return "Create(" + std::string(create.getTypeName()) + ")";
  }
  case grammar::ElementKind::Nest: {
    const auto &nest = static_cast<const grammar::Nest &>(*grammarElement);
    return "Nest(" + std::string(nest.getFeature()) + ")";
  }
  case grammar::ElementKind::Assignment: {
    const auto &assignment =
        static_cast<const grammar::Assignment &>(*grammarElement);
    return assignment_source(assignment);
  }
  case grammar::ElementKind::DataTypeRule:
  case grammar::ElementKind::ParserRule:
  case grammar::ElementKind::TerminalRule: {
    const auto &rule = static_cast<const grammar::AbstractRule &>(*grammarElement);
    return "Rule(" + std::string(rule.getName()) + ")";
  }
  case grammar::ElementKind::AndPredicate:
    return "AndPredicate";
  case grammar::ElementKind::AnyCharacter:
    return "AnyCharacter";
  case grammar::ElementKind::CharacterRange:
    return "CharacterRange";
  case grammar::ElementKind::Group:
    return "Group";
  case grammar::ElementKind::Literal:
    return "Literal";
  case grammar::ElementKind::NotPredicate:
    return "NotPredicate";
  case grammar::ElementKind::OrderedChoice:
    return "OrderedChoice";
  case grammar::ElementKind::Repetition:
    return "Repetition";
  case grammar::ElementKind::UnorderedGroup:
    return "UnorderedGroup";
  case grammar::ElementKind::InfixRule:
    return "InfixRule";
  case grammar::ElementKind::InfixOperator:
    return "InfixOperator";
  }
  return "Unknown";
}

services::JsonValue::Array convert_children(const CstNodeView &node,
                                            const CstJsonConversionOptions &options) {
  services::JsonValue::Array children;
  for (const auto &child : node) {
    children.emplace_back(CstJsonConverter::convert(child, options));
  }
  return children;
}

} // namespace

services::JsonValue
CstJsonConverter::convert(const CstNodeView &node, const Options &options) {
  services::JsonValue::Object object;
  object.emplace("begin", static_cast<std::int64_t>(node.getBegin()));
  object.emplace("end", static_cast<std::int64_t>(node.getEnd()));

  if (options.includeText && node.isLeaf()) {
    object.emplace("text", std::string(node.getText()));
  }
  if (options.includeGrammarSource) {
    object.emplace("grammarSource", grammar_source(node.getGrammarElement()));
  }
  if (options.includeHidden && node.isHidden()) {
    object.emplace("hidden", true);
  }
  if (options.includeRecovered && node.isRecovered()) {
    object.emplace("recovered", true);
  }
  if (!node.isLeaf()) {
    object.emplace("content", convert_children(node, options));
  }

  return services::JsonValue(std::move(object));
}

services::JsonValue
CstJsonConverter::convert(const RootCstNode &root, const Options &options) {
  services::JsonValue::Object object;
  services::JsonValue::Array content;
  for (const auto &child : root) {
    content.emplace_back(convert(child, options));
  }
  object.emplace("content", std::move(content));

  return services::JsonValue(std::move(object));
}

} // namespace pegium::converter
