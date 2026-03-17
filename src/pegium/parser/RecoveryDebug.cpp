#include <pegium/parser/RecoveryDebug.hpp>

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Create.hpp>
#include <pegium/grammar/Group.hpp>
#include <pegium/grammar/InfixRule.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/Nest.hpp>
#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/grammar/ParserRule.hpp>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/grammar/UnorderedGroup.hpp>

namespace pegium::parser::detail {
namespace {

[[nodiscard]] std::int64_t json_int(TextOffset value) noexcept {
  return static_cast<std::int64_t>(value);
}

[[nodiscard]] std::int64_t json_int(std::size_t value) noexcept {
  return static_cast<std::int64_t>(value);
}

[[nodiscard]] std::string
assignment_operator_text(grammar::AssignmentOperator op) {
  switch (op) {
  case grammar::AssignmentOperator::Assign:
    return "=";
  case grammar::AssignmentOperator::Append:
    return "+=";
  case grammar::AssignmentOperator::EnableIf:
    return "?=";
  }
  return "?";
}

[[nodiscard]] std::string
fallback_element_text(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return "null";
  }

  switch (element->getKind()) {
  case grammar::ElementKind::Create:
    return "create";
  case grammar::ElementKind::Nest:
    return "nest";
  case grammar::ElementKind::Assignment:
    return "assignment";
  case grammar::ElementKind::AndPredicate:
    return "and-predicate";
  case grammar::ElementKind::AnyCharacter:
    return "any-character";
  case grammar::ElementKind::CharacterRange:
    return "character-range";
  case grammar::ElementKind::DataTypeRule:
    return "data-type-rule";
  case grammar::ElementKind::Group:
    return "group";
  case grammar::ElementKind::Literal:
    return "literal";
  case grammar::ElementKind::NotPredicate:
    return "not-predicate";
  case grammar::ElementKind::OrderedChoice:
    return "ordered-choice";
  case grammar::ElementKind::ParserRule:
    return "parser-rule";
  case grammar::ElementKind::Repetition:
    return "repetition";
  case grammar::ElementKind::TerminalRule:
    return "terminal-rule";
  case grammar::ElementKind::UnorderedGroup:
    return "unordered-group";
  case grammar::ElementKind::InfixRule:
    return "infix-rule";
  case grammar::ElementKind::InfixOperator:
    return "infix-operator";
  }
  return "element";
}

[[nodiscard]] std::string
join_element_text(std::string separator, std::span<const std::string> parts) {
  if (parts.empty()) {
    return {};
  }

  std::string result = parts.front();
  for (std::size_t index = 1; index < parts.size(); ++index) {
    result += separator;
    result += parts[index];
  }
  return result;
}

[[nodiscard]] std::string
element_text_impl(const grammar::AbstractElement *element,
                  std::unordered_set<const grammar::AbstractElement *> &visited,
                  std::size_t depth);

[[nodiscard]] std::string
child_text(const grammar::AbstractElement *element,
           std::unordered_set<const grammar::AbstractElement *> &visited,
           std::size_t depth) {
  const auto child = element_text_impl(element, visited, depth + 1);
  return child.empty() ? fallback_element_text(element) : child;
}

[[nodiscard]] std::string
sequence_text(std::size_t size,
              const auto &get_element,
              std::string_view separator,
              std::unordered_set<const grammar::AbstractElement *> &visited,
              std::size_t depth, std::string_view emptyLabel) {
  std::vector<std::string> children;
  children.reserve(size);
  for (std::size_t index = 0; index < size; ++index) {
    const auto *child = get_element(index);
    if (const auto text = child_text(child, visited, depth); !text.empty()) {
      children.push_back(text);
    }
  }
  if (children.empty()) {
    return std::string(emptyLabel);
  }
  return join_element_text(std::string(separator), children);
}

[[nodiscard]] std::string
element_text_impl(const grammar::AbstractElement *element,
                  std::unordered_set<const grammar::AbstractElement *> &visited,
                  std::size_t depth) {
  if (element == nullptr) {
    return {};
  }
  if (depth >= 8 || !visited.insert(element).second) {
    return fallback_element_text(element);
  }

  switch (element->getKind()) {
  case grammar::ElementKind::Literal: {
    const auto value = static_cast<const grammar::Literal *>(element)->getValue();
    return value.empty() ? "literal" : "`" + std::string(value) + "`";
  }
  case grammar::ElementKind::TerminalRule:
  case grammar::ElementKind::DataTypeRule:
  case grammar::ElementKind::ParserRule:
  case grammar::ElementKind::InfixRule:
    return std::string(
        static_cast<const grammar::AbstractRule *>(element)->getName());
  case grammar::ElementKind::Assignment: {
    const auto *assignment = static_cast<const grammar::Assignment *>(element);
    return std::string(assignment->getFeature()) +
           assignment_operator_text(assignment->getOperator()) +
           child_text(assignment->getElement(), visited, depth);
  }
  case grammar::ElementKind::AndPredicate: {
    const auto *predicate = static_cast<const grammar::AndPredicate *>(element);
    return "&" + child_text(predicate->getElement(), visited, depth);
  }
  case grammar::ElementKind::NotPredicate: {
    const auto *predicate = static_cast<const grammar::NotPredicate *>(element);
    return "!" + child_text(predicate->getElement(), visited, depth);
  }
  case grammar::ElementKind::Repetition: {
    const auto *repetition = static_cast<const grammar::Repetition *>(element);
    const auto child = child_text(repetition->getElement(), visited, depth);
    if (repetition->getMin() == 0 &&
        repetition->getMax() == std::numeric_limits<std::size_t>::max()) {
      return child + "*";
    }
    if (repetition->getMin() == 1 &&
        repetition->getMax() == std::numeric_limits<std::size_t>::max()) {
      return child + "+";
    }
    if (repetition->getMin() == 0 && repetition->getMax() == 1) {
      return child + "?";
    }
    return child + "{" + std::to_string(repetition->getMin()) + "," +
           std::to_string(repetition->getMax()) + "}";
  }
  case grammar::ElementKind::Group: {
    const auto *group = static_cast<const grammar::Group *>(element);
    return sequence_text(
        group->size(), [group](std::size_t index) { return group->get(index); },
        " ", visited, depth, "group");
  }
  case grammar::ElementKind::OrderedChoice: {
    const auto *choice = static_cast<const grammar::OrderedChoice *>(element);
    return sequence_text(
        choice->size(),
        [choice](std::size_t index) { return choice->get(index); }, " | ",
        visited, depth, "ordered-choice");
  }
  case grammar::ElementKind::UnorderedGroup: {
    const auto *group = static_cast<const grammar::UnorderedGroup *>(element);
    return sequence_text(
        group->size(), [group](std::size_t index) { return group->get(index); },
        " & ", visited, depth, "unordered-group");
  }
  case grammar::ElementKind::Create: {
    const auto *create = static_cast<const grammar::Create *>(element);
    return "create<" + std::string(create->getTypeName()) + ">";
  }
  case grammar::ElementKind::Nest: {
    const auto *nest = static_cast<const grammar::Nest *>(element);
    return "nest<" + std::string(nest->getTypeName()) + ">." +
           std::string(nest->getFeature());
  }
  case grammar::ElementKind::InfixOperator: {
    const auto *operatorElement =
        static_cast<const grammar::InfixOperator *>(element);
    const auto assoc =
        operatorElement->getAssociativity() ==
                grammar::InfixOperator::Associativity::Left
            ? "left"
            : "right";
    return std::string(assoc) + " " +
           child_text(operatorElement->getOperator(), visited, depth);
  }
  default:
    return fallback_element_text(element);
  }
}

[[nodiscard]] std::string element_text(const grammar::AbstractElement *element) {
  std::unordered_set<const grammar::AbstractElement *> visited;
  return element_text_impl(element, visited, 0);
}

[[nodiscard]] services::JsonValue
element_to_json(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return nullptr;
  }

  return services::JsonValue::Object{
      {"kind", json_int(static_cast<std::uint32_t>(element->getKind()))},
      {"text", element_text(element)},
  };
}

[[nodiscard]] services::JsonValue
parse_diagnostic_to_json(const ParseDiagnostic &diagnostic) {
  std::ostringstream stream;
  stream << diagnostic.kind;
  return services::JsonValue::Object{
      {"element", element_to_json(diagnostic.element)},
      {"kind", stream.str()},
      {"offset", json_int(diagnostic.offset)},
  };
}

[[nodiscard]] services::JsonValue
failure_leaf_to_json(const FailureLeaf &leaf) {
  return services::JsonValue::Object{
      {"beginOffset", json_int(leaf.beginOffset)},
      {"element", element_to_json(leaf.element)},
      {"endOffset", json_int(leaf.endOffset)},
  };
}

[[nodiscard]] services::JsonValue
edit_trace_to_json(const EditTrace &trace) {
  return services::JsonValue::Object{
      {"codepointDeleteCount", json_int(trace.codepointDeleteCount)},
      {"deleteCount", json_int(trace.deleteCount)},
      {"diagnosticCount", json_int(trace.diagnosticCount)},
      {"editCost", json_int(trace.editCost)},
      {"editCount", json_int(trace.editCount)},
      {"editSpan", json_int(trace.editSpan)},
      {"firstEditOffset", json_int(trace.firstEditOffset)},
      {"hasEdits", trace.hasEdits},
      {"insertCount", json_int(trace.insertCount)},
      {"lastEditOffset", json_int(trace.lastEditOffset)},
      {"replaceCount", json_int(trace.replaceCount)},
      {"tokenDeleteCount", json_int(trace.tokenDeleteCount)},
      {"tokenInsertCount", json_int(trace.tokenInsertCount)},
  };
}

[[nodiscard]] services::JsonValue
score_to_json(const RecoveryScore &score) {
  return services::JsonValue::Object{
      {"credible", score.credible},
      {"diagnosticCount", json_int(score.diagnosticCount)},
      {"editCost", json_int(score.editCost)},
      {"editSpan", json_int(score.editSpan)},
      {"entryRuleMatched", score.entryRuleMatched},
      {"firstEditOffset", json_int(score.firstEditOffset)},
      {"fullMatch", score.fullMatch},
      {"maxCursorOffset", json_int(score.maxCursorOffset)},
      {"parsedLength", json_int(score.parsedLength)},
      {"stable", score.stable},
  };
}

[[nodiscard]] services::JsonValue
attempt_spec_to_json(const RecoveryAttemptSpec &spec) {
  services::JsonValue::Object object{
      {"windows", recovery_windows_to_json(spec.windows)},
  };
  return object;
}

} // namespace

std::string_view
recovery_attempt_status_name(RecoveryAttemptStatus status) noexcept {
  switch (status) {
  case RecoveryAttemptStatus::StrictFailure:
    return "StrictFailure";
  case RecoveryAttemptStatus::RecoveredButNotCredible:
    return "RecoveredButNotCredible";
  case RecoveryAttemptStatus::Credible:
    return "Credible";
  case RecoveryAttemptStatus::Stable:
    return "Stable";
  case RecoveryAttemptStatus::Selected:
    return "Selected";
  }
  return "Unknown";
}

services::JsonValue
strict_parse_summary_to_json(const StrictParseSummary &summary) {
  return services::JsonValue::Object{
      {"entryRuleMatched", summary.entryRuleMatched},
      {"fullMatch", summary.fullMatch},
      {"inputSize", json_int(summary.inputSize)},
      {"maxCursorOffset", json_int(summary.maxCursorOffset)},
      {"parsedLength", json_int(summary.parsedLength)},
  };
}

services::JsonValue failure_snapshot_to_json(const FailureSnapshot &snapshot) {
  services::JsonValue::Array leaves;
  leaves.reserve(snapshot.failureLeafHistory.size());
  for (const auto &leaf : snapshot.failureLeafHistory) {
    leaves.emplace_back(failure_leaf_to_json(leaf));
  }

  services::JsonValue::Object object{
      {"failureLeafHistory", std::move(leaves)},
      {"failureTokenIndex", json_int(snapshot.failureTokenIndex)},
      {"hasFailureToken", snapshot.hasFailureToken},
      {"maxCursorOffset", json_int(snapshot.maxCursorOffset)},
  };
  if (snapshot.hasFailureToken &&
      snapshot.failureTokenIndex < snapshot.failureLeafHistory.size()) {
    object.emplace(
        "failureToken",
        failure_leaf_to_json(snapshot.failureLeafHistory[snapshot.failureTokenIndex]));
  } else {
    object.emplace("failureToken", nullptr);
  }
  return object;
}

services::JsonValue recovery_window_to_json(const RecoveryWindow &window) {
  return services::JsonValue::Object{
      {"backwardTokenCount", json_int(window.tokenCount)},
      {"beginOffset", json_int(window.beginOffset)},
      {"forwardTokenCount", json_int(window.tokenCount)},
      {"maxCursorOffset", json_int(window.maxCursorOffset)},
      {"visibleLeafBeginIndex", json_int(window.visibleLeafBeginIndex)},
  };
}

services::JsonValue
recovery_windows_to_json(std::span<const RecoveryWindow> windows) {
  services::JsonValue::Array array;
  array.reserve(windows.size());
  for (const auto &window : windows) {
    array.emplace_back(recovery_window_to_json(window));
  }
  return array;
}

services::JsonValue
recovery_attempt_to_json(const RecoveryAttempt &attempt,
                         const RecoveryAttemptSpec *spec) {
  services::JsonValue::Array diagnostics;
  diagnostics.reserve(attempt.parseDiagnostics.size());
  for (const auto &diagnostic : attempt.parseDiagnostics) {
    diagnostics.emplace_back(parse_diagnostic_to_json(diagnostic));
  }

  services::JsonValue::Object object{
      {"completedRecoveryWindows", json_int(attempt.completedRecoveryWindows)},
      {"editCost", json_int(attempt.editCost)},
      {"editCount", json_int(attempt.editCount)},
      {"editFloorOffset", json_int(attempt.editFloorOffset)},
      {"editTrace", edit_trace_to_json(attempt.editTrace)},
      {"entryRuleMatched", attempt.entryRuleMatched},
      {"failureSnapshot",
       attempt.failureSnapshot.has_value()
           ? failure_snapshot_to_json(*attempt.failureSnapshot)
           : services::JsonValue(nullptr)},
      {"fullMatch", attempt.fullMatch},
      {"maxCursorOffset", json_int(attempt.maxCursorOffset)},
      {"parseDiagnostics", std::move(diagnostics)},
      {"parsedLength", json_int(attempt.parsedLength)},
      {"reachedRecoveryTarget", attempt.reachedRecoveryTarget},
      {"score", score_to_json(attempt.score)},
      {"stableAfterRecovery", attempt.stableAfterRecovery},
      {"status", std::string(recovery_attempt_status_name(attempt.status))},
  };
  if (spec != nullptr) {
    object.emplace("spec", attempt_spec_to_json(*spec));
  }
  return object;
}

} // namespace pegium::parser::detail
