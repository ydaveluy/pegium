#include <pegium/core/parser/RecoveryDebug.hpp>

#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/AndPredicate.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/Create.hpp>
#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/grammar/Nest.hpp>
#include <pegium/core/grammar/NotPredicate.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/grammar/UnorderedGroup.hpp>

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
  using enum grammar::AssignmentOperator;
  switch (op) {
  case Assign:
    return "=";
  case Append:
    return "+=";
  case EnableIf:
    return "?=";
  }
  return "?";
}

[[nodiscard]] std::string
fallback_element_text(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return "null";
  }

  using enum grammar::ElementKind;
  switch (element->getKind()) {
  case Create:
    return "create";
  case Nest:
    return "nest";
  case Assignment:
    return "assignment";
  case AndPredicate:
    return "and-predicate";
  case AnyCharacter:
    return "any-character";
  case CharacterRange:
    return "character-range";
  case DataTypeRule:
    return "data-type-rule";
  case Group:
    return "group";
  case Literal:
    return "literal";
  case NotPredicate:
    return "not-predicate";
  case OrderedChoice:
    return "ordered-choice";
  case ParserRule:
    return "parser-rule";
  case Repetition:
    return "repetition";
  case TerminalRule:
    return "terminal-rule";
  case UnorderedGroup:
    return "unordered-group";
  case InfixRule:
    return "infix-rule";
  case InfixOperator:
    return "infix-operator";
  }
  return "element";
}

[[nodiscard]] std::string
join_element_text(const std::string &separator,
                  std::span<const std::string> parts) {
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

  using enum grammar::ElementKind;
  switch (element->getKind()) {
  case Literal: {
    const auto value = static_cast<const grammar::Literal *>(element)->getValue();
    return value.empty() ? "literal" : "`" + std::string(value) + "`";
  }
  case TerminalRule:
  case DataTypeRule:
  case ParserRule:
  case InfixRule:
    return std::string(
        static_cast<const grammar::AbstractRule *>(element)->getName());
  case Assignment: {
    const auto *assignment = static_cast<const grammar::Assignment *>(element);
    return std::string(assignment->getFeature()) +
           assignment_operator_text(assignment->getOperator()) +
           child_text(assignment->getElement(), visited, depth);
  }
  case AndPredicate: {
    const auto *predicate = static_cast<const grammar::AndPredicate *>(element);
    return "&" + child_text(predicate->getElement(), visited, depth);
  }
  case NotPredicate: {
    const auto *predicate = static_cast<const grammar::NotPredicate *>(element);
    return "!" + child_text(predicate->getElement(), visited, depth);
  }
  case Repetition: {
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
  case Group: {
    const auto *group = static_cast<const grammar::Group *>(element);
    return sequence_text(
        group->size(), [group](std::size_t index) { return group->get(index); },
        " ", visited, depth, "group");
  }
  case OrderedChoice: {
    const auto *choice = static_cast<const grammar::OrderedChoice *>(element);
    return sequence_text(
        choice->size(),
        [choice](std::size_t index) { return choice->get(index); }, " | ",
        visited, depth, "ordered-choice");
  }
  case UnorderedGroup: {
    const auto *group = static_cast<const grammar::UnorderedGroup *>(element);
    return sequence_text(
        group->size(), [group](std::size_t index) { return group->get(index); },
        " & ", visited, depth, "unordered-group");
  }
  case Create: {
    const auto *create = static_cast<const grammar::Create *>(element);
    return "create<" + std::string(create->getTypeName()) + ">";
  }
  case Nest: {
    const auto *nest = static_cast<const grammar::Nest *>(element);
    return "nest<" + std::string(nest->getTypeName()) + ">." +
           std::string(nest->getFeature());
  }
  case InfixOperator: {
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

[[nodiscard]] pegium::JsonValue
element_to_json(const grammar::AbstractElement *element) {
  if (element == nullptr) {
    return nullptr;
  }

  return pegium::JsonValue::Object{
      {"kind", json_int(static_cast<std::uint32_t>(element->getKind()))},
      {"text", element_text(element)},
  };
}

[[nodiscard]] pegium::JsonValue
syntax_entry_to_json(const auto &entry) {
  std::ostringstream stream;
  stream << entry.kind;
  return pegium::JsonValue::Object{
      {"element", element_to_json(entry.element)},
      {"kind", stream.str()},
      {"offset", json_int(entry.offset)},
  };
}

[[nodiscard]] pegium::JsonValue
failure_leaf_to_json(const FailureLeaf &leaf) {
  return pegium::JsonValue::Object{
      {"beginOffset", json_int(leaf.beginOffset)},
      {"element", element_to_json(leaf.element)},
      {"endOffset", json_int(leaf.endOffset)},
  };
}

[[nodiscard]] pegium::JsonValue
edit_trace_to_json(const EditTrace &trace) {
  return pegium::JsonValue::Object{
      {"codepointDeleteCount", json_int(trace.codepointDeleteCount)},
      {"deleteCount", json_int(trace.deleteCount)},
      {"entryCount", json_int(trace.entryCount)},
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

[[nodiscard]] pegium::JsonValue
score_to_json(const RecoveryScore &score) {
  return pegium::JsonValue::Object{
      {"credible", score.selection.credible},
      {"entryCount", json_int(score.edits.entryCount)},
      {"editCost", json_int(score.edits.editCost)},
      {"editSpan", json_int(score.edits.editSpan)},
      {"entryRuleMatched", score.selection.entryRuleMatched},
      {"firstEditOffset", json_int(score.edits.firstEditOffset)},
      {"fullMatch", score.selection.fullMatch},
      {"maxCursorOffset", json_int(score.progress.maxCursorOffset)},
      {"parsedLength", json_int(score.progress.parsedLength)},
      {"stable", score.selection.stable},
  };
}

[[nodiscard]] pegium::JsonValue
order_key_to_json(const NormalizedRecoveryOrderKey &key) {
  return pegium::JsonValue::Object{
      {"prefix",
       pegium::JsonValue::Object{
           {"credible", key.prefix.credible},
           {"entryRuleMatched", key.prefix.entryRuleMatched},
           {"firstEditOffset", json_int(key.prefix.firstEditOffset)},
           {"fullMatch", key.prefix.fullMatch},
           {"stable", key.prefix.stable},
       }},
      {"continuation",
       pegium::JsonValue::Object{
           {"anchorQuality", json_int(static_cast<std::uint32_t>(
                                 key.continuation.anchorQuality))},
           {"consumedVisible", json_int(key.continuation.consumedVisible)},
           {"continuesAfterFirstEdit",
            key.continuation.continuesAfterFirstEdit},
           {"cursorOffset", json_int(key.continuation.cursorOffset)},
           {"postSkipCursorOffset",
            json_int(key.continuation.postSkipCursorOffset)},
           {"strength",
            json_int(static_cast<TextOffset>(key.continuation.strength))},
       }},
      {"safety",
       pegium::JsonValue::Object{
           {"matched", key.safety.matched},
           {"overflowed", key.safety.overflowed},
           {"replaySafe", key.safety.replaySafe},
           {"strategyPriority",
            json_int(static_cast<TextOffset>(key.safety.strategyPriority))},
       }},
      {"edits",
       pegium::JsonValue::Object{
           {"entryCount", json_int(key.edits.entryCount)},
           {"distance", json_int(key.edits.distance)},
           {"editCost", json_int(key.edits.editCost)},
           {"editCount", json_int(key.edits.editCount)},
           {"editSpan", json_int(key.edits.editSpan)},
           {"operationCount", json_int(key.edits.operationCount)},
           {"primaryRankCost", json_int(key.edits.primaryRankCost)},
           {"secondaryRankCost", json_int(key.edits.secondaryRankCost)},
           {"substitutionCount", json_int(key.edits.substitutionCount)},
       }},
      {"progress",
       pegium::JsonValue::Object{
           {"cursorOffset", json_int(key.progress.cursorOffset)},
           {"maxCursorOffset", json_int(key.progress.maxCursorOffset)},
           {"parsedLength", json_int(key.progress.parsedLength)},
       }},
  };
}

[[nodiscard]] pegium::JsonValue
attempt_spec_to_json(const RecoveryAttemptSpec &spec) {
  pegium::JsonValue::Object object{
      {"windows", recovery_windows_to_json(spec.windows)},
  };
  return object;
}

} // namespace

std::string_view
recovery_attempt_status_name(RecoveryAttemptStatus status) noexcept {
  using enum RecoveryAttemptStatus;
  switch (status) {
  case StrictFailure:
    return "StrictFailure";
  case RecoveredButNotCredible:
    return "RecoveredButNotCredible";
  case Credible:
    return "Credible";
  case Stable:
    return "Stable";
  }
  return "Unknown";
}

pegium::JsonValue
strict_parse_summary_to_json(const StrictParseSummary &summary) {
  return pegium::JsonValue::Object{
      {"entryRuleMatched", summary.entryRuleMatched},
      {"fullMatch", summary.fullMatch},
      {"inputSize", json_int(summary.inputSize)},
      {"maxCursorOffset", json_int(summary.maxCursorOffset)},
      {"parsedLength", json_int(summary.parsedLength)},
  };
}

pegium::JsonValue failure_snapshot_to_json(const FailureSnapshot &snapshot) {
  pegium::JsonValue::Array leaves;
  leaves.reserve(snapshot.failureLeafHistory.size());
  for (const auto &leaf : snapshot.failureLeafHistory) {
    leaves.emplace_back(failure_leaf_to_json(leaf));
  }

  pegium::JsonValue::Object object{
      {"failureLeafHistory", std::move(leaves)},
      {"failureTokenIndex", json_int(snapshot.failureTokenIndex)},
      {"hasFailureToken", snapshot.hasFailureToken},
      {"maxCursorOffset", json_int(snapshot.maxCursorOffset)},
  };
  if (snapshot.hasFailureToken &&
      snapshot.failureTokenIndex < snapshot.failureLeafHistory.size()) {
    object.try_emplace(
        "failureToken",
        failure_leaf_to_json(snapshot.failureLeafHistory[snapshot.failureTokenIndex]));
  } else {
    object.try_emplace("failureToken", nullptr);
  }
  return object;
}

pegium::JsonValue recovery_window_to_json(const RecoveryWindow &window) {
  return pegium::JsonValue::Object{
      {"backwardTokenCount", json_int(window.tokenCount)},
      {"beginOffset", json_int(window.beginOffset)},
      {"editFloorOffset", json_int(window.editFloorOffset)},
      {"forwardTokenCount", json_int(window.forwardTokenCount)},
      {"hasStablePrefix", window.hasStablePrefix},
      {"maxCursorOffset", json_int(window.maxCursorOffset)},
      {"stablePrefixOffset", json_int(window.stablePrefixOffset)},
      {"visibleLeafBeginIndex", json_int(window.visibleLeafBeginIndex)},
  };
}

pegium::JsonValue
recovery_windows_to_json(std::span<const RecoveryWindow> windows) {
  pegium::JsonValue::Array array;
  array.reserve(windows.size());
  for (const auto &window : windows) {
    array.emplace_back(recovery_window_to_json(window));
  }
  return array;
}

pegium::JsonValue
recovery_attempt_to_json(const RecoveryAttempt &attempt,
                         const RecoveryAttemptSpec *spec) {
  pegium::JsonValue::Array recoveryEdits;
  recoveryEdits.reserve(attempt.recoveryEdits.size());
  for (const auto &edit : attempt.recoveryEdits) {
    recoveryEdits.emplace_back(syntax_entry_to_json(edit));
  }

  const auto parseDiagnostics =
      materialize_syntax_diagnostics(attempt.recoveryEdits);
  pegium::JsonValue::Array diagnostics;
  diagnostics.reserve(parseDiagnostics.size());
  for (const auto &diagnostic : parseDiagnostics) {
    diagnostics.emplace_back(syntax_entry_to_json(diagnostic));
  }

  pegium::JsonValue::Object object{
      {"completedRecoveryWindows", json_int(attempt.completedRecoveryWindows)},
      {"editCost", json_int(attempt.editCost)},
      {"editCount", json_int(attempt.editCount)},
      {"editTrace", edit_trace_to_json(attempt.editTrace)},
      {"entryRuleMatched", attempt.entryRuleMatched},
      {"failureSnapshot",
       attempt.failureSnapshot.has_value()
           ? failure_snapshot_to_json(*attempt.failureSnapshot)
           : pegium::JsonValue(nullptr)},
      {"fullMatch", attempt.fullMatch},
      {"maxCursorOffset", json_int(attempt.maxCursorOffset)},
      {"orderKey",
       order_key_to_json(recovery_attempt_order_key(attempt.score))},
      {"parseDiagnostics", std::move(diagnostics)},
      {"recoveryEdits", std::move(recoveryEdits)},
      {"parsedLength", json_int(attempt.parsedLength)},
      {"reachedRecoveryTarget", attempt.reachedRecoveryTarget},
      {"score", score_to_json(attempt.score)},
      {"hasStablePrefix", attempt.hasStablePrefix},
      {"stableAfterRecovery", attempt.stableAfterRecovery},
      {"stablePrefixOffset", json_int(attempt.stablePrefixOffset)},
      {"status", std::string(recovery_attempt_status_name(attempt.status))},
  };
  if (!attempt.replayWindows.empty()) {
    object.try_emplace("replayWindows",
                      recovery_windows_to_json(attempt.replayWindows));
  }
  if (spec != nullptr) {
    object.try_emplace("spec", attempt_spec_to_json(*spec));
  }
  return object;
}

pegium::JsonValue
recovery_search_run_to_json(const RecoverySearchRunResult &run) {
  return pegium::JsonValue::Object{
      {"failureVisibleCursorOffset", json_int(run.failureVisibleCursorOffset)},
      {"recoveryAttemptRuns", json_int(run.recoveryAttemptRuns)},
      {"recoveryWindowsTried", json_int(run.recoveryWindowsTried)},
      {"selectedAttempt", recovery_attempt_to_json(run.selectedAttempt)},
      {"selectedWindows", recovery_windows_to_json(run.selectedWindows)},
      {"strictParseRuns", json_int(run.strictParseRuns)},
  };
}

} // namespace pegium::parser::detail
