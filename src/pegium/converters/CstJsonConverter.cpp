#include <pegium/converters/CstJsonConverter.hpp>

#include <cassert>
#include <sstream>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/Create.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/grammar/Nest.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>

namespace pegium::converter {
namespace {

std::string assignment_operator_source(grammar::AssignmentOperator op) {
  using enum grammar::AssignmentOperator;
  switch (op) {
  case Assign:
    return "=";
  case Append:
    return "+=";
  case EnableIf:
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
  using enum grammar::ElementKind;
  switch (element.getKind()) {
  case DataTypeRule:
  case ParserRule:
  case TerminalRule:
    return std::string(
        static_cast<const grammar::AbstractRule &>(element).getName());
  case Literal: {
    std::ostringstream stream;
    stream << static_cast<const grammar::Literal &>(element);
    return stream.str();
  }
  case OrderedChoice:
    return ordered_choice_source(
        static_cast<const grammar::OrderedChoice &>(element));
  case Create:
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
  using enum grammar::ElementKind;
  switch (grammarElement->getKind()) {
  case Create: {
    const auto &create = static_cast<const grammar::Create &>(*grammarElement);
    return "Create(" + std::string(create.getTypeName()) + ")";
  }
  case Nest: {
    const auto &nest = static_cast<const grammar::Nest &>(*grammarElement);
    return "Nest(" + std::string(nest.getFeature()) + ")";
  }
  case Assignment: {
    const auto &assignment =
        static_cast<const grammar::Assignment &>(*grammarElement);
    return assignment_source(assignment);
  }
  case DataTypeRule:
  case ParserRule:
  case TerminalRule: {
    const auto &rule = static_cast<const grammar::AbstractRule &>(*grammarElement);
    return "Rule(" + std::string(rule.getName()) + ")";
  }
  case AndPredicate:
    return "AndPredicate";
  case AnyCharacter:
    return "AnyCharacter";
  case CharacterRange:
    return "CharacterRange";
  case Group:
    return "Group";
  case Literal:
    return "Literal";
  case NotPredicate:
    return "NotPredicate";
  case OrderedChoice:
    return "OrderedChoice";
  case Repetition:
    return "Repetition";
  case UnorderedGroup:
    return "UnorderedGroup";
  case InfixRule:
    return "InfixRule";
  case InfixOperator:
    return "InfixOperator";
  }
  return "Unknown";
}

pegium::JsonValue::Array convert_children(const CstNodeView &node,
                                            const CstJsonConversionOptions &options) {
  pegium::JsonValue::Array children;
  for (const auto &child : node) {
    children.emplace_back(CstJsonConverter::convert(child, options));
  }
  return children;
}

} // namespace

pegium::JsonValue
CstJsonConverter::convert(const CstNodeView &node, const Options &options) {
  pegium::JsonValue::Object object;
  object.try_emplace("begin", static_cast<std::int64_t>(node.getBegin()));
  object.try_emplace("end", static_cast<std::int64_t>(node.getEnd()));

  if (options.includeText && node.isLeaf()) {
    object.try_emplace("text", std::string(node.getText()));
  }
  if (options.includeGrammarSource) {
    object.try_emplace("grammarSource", grammar_source(node.getGrammarElement()));
  }
  if (options.includeHidden && node.isHidden()) {
    object.try_emplace("hidden", true);
  }
  if (options.includeRecovered && node.isRecovered()) {
    object.try_emplace("recovered", true);
  }
  if (!node.isLeaf()) {
    object.try_emplace("content", convert_children(node, options));
  }

  return pegium::JsonValue(std::move(object));
}

pegium::JsonValue
CstJsonConverter::convert(const RootCstNode &root, const Options &options) {
  pegium::JsonValue::Object object;
  pegium::JsonValue::Array content;
  for (const auto &child : root) {
    content.emplace_back(convert(child, options));
  }
  object.try_emplace("content", std::move(content));

  return pegium::JsonValue(std::move(object));
}

} // namespace pegium::converter
