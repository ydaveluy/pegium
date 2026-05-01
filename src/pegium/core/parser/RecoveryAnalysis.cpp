#include <pegium/core/parser/RecoveryAnalysis.hpp>

#include <algorithm>
#include <utility>

#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/DataTypeRule.hpp>
#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/grammar/UnorderedGroup.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>

namespace pegium::parser::detail {

namespace {

template <typename ContextFactory>
[[nodiscard]] StrictParseResult
run_strict_parse_with_context(const grammar::ParserRule &entryRule,
                              const Skipper &skipper,
                              const text::TextSnapshot &text,
                              const utils::CancellationToken &cancelToken,
                              ContextFactory &&makeContext) {
  StrictParseResult result;
  result.summary.inputSize = static_cast<TextOffset>(text.size());

  auto cst = std::make_unique<RootCstNode>(text);
  CstBuilder builder(*cst);
  auto parseCtx = makeContext(builder, skipper, cancelToken);
  parseCtx.skip();
  const auto attemptCheckpoint = parseCtx.mark();

  result.summary.entryRuleMatched = entryRule.rule(parseCtx);
  const auto failureParsedLength = parseCtx.cursorOffset();
  const auto failureMaxCursorOffset = parseCtx.maxCursorOffset();
  if (!result.summary.entryRuleMatched) {
    parseCtx.rewind(attemptCheckpoint);
  }

  parseCtx.skip();
  result.summary.parsedLength = result.summary.entryRuleMatched
                                    ? parseCtx.cursorOffset()
                                    : failureParsedLength;
  result.summary.lastVisibleCursorOffset = parseCtx.lastVisibleCursorOffset();
  result.summary.maxCursorOffset = result.summary.entryRuleMatched
                                       ? parseCtx.maxCursorOffset()
                                       : failureMaxCursorOffset;
  result.summary.fullMatch =
      result.summary.entryRuleMatched &&
      result.summary.parsedLength == result.summary.inputSize;
  (void)builder.getRootCstNode();
  result.cst = std::move(cst);
  return result;
}

[[nodiscard]] bool
has_committed_visible_prefix(const FailureSnapshot &snapshot,
                             TextOffset parsedLength) noexcept {
  if (parsedLength == 0) {
    return false;
  }
  return std::ranges::any_of(snapshot.failureLeafHistory,
                             [parsedLength](const FailureLeaf &leaf) {
                               return leaf.endOffset <= parsedLength;
                             });
}

} // namespace

bool should_fallback_to_parsed_length_snapshot(
    const FailureSnapshot &snapshot, TextOffset parsedLength) noexcept {
  if (!has_committed_visible_prefix(snapshot, parsedLength)) {
    return false;
  }
  if (!snapshot.hasFailureToken ||
      snapshot.failureTokenIndex >= snapshot.failureLeafHistory.size()) {
    return false;
  }
  const auto &failureLeaf =
      snapshot.failureLeafHistory[snapshot.failureTokenIndex];
  if (failureLeaf.beginOffset <= parsedLength ||
      snapshot.failureTokenIndex == 0) {
    return false;
  }
  const auto &previousLeaf =
      snapshot.failureLeafHistory[snapshot.failureTokenIndex - 1];
  return previousLeaf.endOffset == failureLeaf.beginOffset &&
         previousLeaf.beginOffset > parsedLength;
}

FailureSnapshot FailureHistoryRecorder::snapshot(TextOffset maxCursorOffset) const {
  FailureSnapshot snapshot{
      .maxCursorOffset = maxCursorOffset,
      .failureLeafHistory =
          std::vector<FailureLeaf>(_visibleLeaves.begin(),
                                   _visibleLeaves.begin() +
                                       static_cast<std::ptrdiff_t>(
                                           _furthestVisibleLeafCount))};
  finalize_failure_token_index(snapshot);
  return snapshot;
}

StrictParseResult StrictFailureEngine::runStrictParse(
    const grammar::ParserRule &entryRule, const Skipper &skipper,
    const text::TextSnapshot &text,
    const utils::CancellationToken &cancelToken,
    FailureHistoryRecorder *failureRecorder) const {
  if (failureRecorder == nullptr) {
    return run_strict_parse_with_context(
        entryRule, skipper, text, cancelToken,
        [](CstBuilder &builder, const Skipper &localSkipper,
           const utils::CancellationToken &localCancelToken) {
          return ParseContext{builder, localSkipper, localCancelToken};
        });
  }
  return run_strict_parse_with_context(
      entryRule, skipper, text, cancelToken,
      [failureRecorder](CstBuilder &builder, const Skipper &localSkipper,
                        const utils::CancellationToken &localCancelToken) {
        return TrackedParseContext{builder, localSkipper, *failureRecorder,
                                   localCancelToken};
      });
}

StrictFailureEngineResult
run_strict_parse_with_failure_snapshot(
    const grammar::ParserRule &entryRule, const Skipper &skipper,
    const text::TextSnapshot &text,
    const utils::CancellationToken &cancelToken) {
  StrictFailureEngineResult result;
  FailureHistoryRecorder recorder(text.view().data());
  const StrictFailureEngine engine;
  result.strictResult =
      engine.runStrictParse(entryRule, skipper, text, cancelToken, &recorder);
  const auto &summary = result.strictResult.summary;
  const auto trackedMaxCursorOffset =
      std::max(summary.maxCursorOffset, recorder.furthestOffset());
  auto snapshot = recorder.snapshot(trackedMaxCursorOffset);
  if (summary.parsedLength < trackedMaxCursorOffset &&
      should_fallback_to_parsed_length_snapshot(snapshot,
                                                summary.parsedLength)) {
    snapshot = recorder.snapshot(summary.parsedLength);
  }
  result.snapshot = std::move(snapshot);
  return result;
}

FailureSnapshot snapshot_from_committed_cst(const RootCstNode &cst,
                                            TextOffset maxCursorOffset) noexcept {
  FailureSnapshot snapshot{.maxCursorOffset = maxCursorOffset,
                           .failureLeafHistory = {}};
  for (NodeId id = 0;; ++id) {
    const auto node = cst.get(id);
    if (!node.valid()) {
      break;
    }
    if (node.isHidden() || !node.isLeaf() || node.getText().empty()) {
      continue;
    }
    snapshot.failureLeafHistory.push_back(
        {.beginOffset = node.getBegin(),
         .endOffset = node.getEnd(),
         .element = node.getGrammarElement()});
  }
  finalize_failure_token_index(snapshot);
  return snapshot;
}

void finalize_failure_token_index(FailureSnapshot &snapshot) noexcept {
  snapshot.failureTokenIndex = 0;
  snapshot.hasFailureToken = false;

  for (std::size_t index = 0; index < snapshot.failureLeafHistory.size(); ++index) {
    const auto &leaf = snapshot.failureLeafHistory[index];
    if (leaf.beginOffset <= snapshot.maxCursorOffset &&
        snapshot.maxCursorOffset < leaf.endOffset) {
      snapshot.failureTokenIndex = index;
      snapshot.hasFailureToken = true;
      return;
    }
  }

  for (std::size_t index = snapshot.failureLeafHistory.size(); index > 0; --index) {
    const auto &leaf = snapshot.failureLeafHistory[index - 1];
    if (leaf.endOffset <= snapshot.maxCursorOffset) {
      snapshot.failureTokenIndex = index - 1;
      snapshot.hasFailureToken = true;
      return;
    }
  }
}

RecoveryWindow compute_recovery_window(const FailureSnapshot &snapshot,
                                       std::uint32_t backwardTokenCount,
                                       std::uint32_t forwardTokenCount,
                                       TextOffset stablePrefixFloorOffset) noexcept {
  const auto effectiveMaxCursorOffset =
      snapshot.failureLeafHistory.empty()
          ? snapshot.maxCursorOffset
          : std::max(snapshot.maxCursorOffset,
                     snapshot.failureLeafHistory.back().endOffset);
  RecoveryWindow window{
      .beginOffset = effectiveMaxCursorOffset,
      .editFloorOffset = effectiveMaxCursorOffset,
      .maxCursorOffset = effectiveMaxCursorOffset,
      .tokenCount = backwardTokenCount,
      .forwardTokenCount = forwardTokenCount,
  };
  if (snapshot.failureLeafHistory.empty() || backwardTokenCount == 0 ||
      !snapshot.hasFailureToken) {
    return window;
  }

  const auto beginIndex =
      snapshot.hasFailureToken
          ? static_cast<std::uint32_t>(
                snapshot.failureTokenIndex > backwardTokenCount
                    ? snapshot.failureTokenIndex - backwardTokenCount
                    : 0)
          : 0u;
  const auto clampedStablePrefixFloorOffset =
      std::min(stablePrefixFloorOffset, effectiveMaxCursorOffset);
  const auto activationBeginOffset =
      snapshot.failureLeafHistory[beginIndex].beginOffset;
  const auto replayBeginOffset =
      clampedStablePrefixFloorOffset == 0
          ? activationBeginOffset
          : std::min(activationBeginOffset, clampedStablePrefixFloorOffset);

  window.visibleLeafBeginIndex = beginIndex;
  if (replayBeginOffset < activationBeginOffset) {
    for (std::uint32_t index = 0; index < beginIndex; ++index) {
      if (snapshot.failureLeafHistory[index].endOffset > replayBeginOffset) {
        window.visibleLeafBeginIndex = index;
        break;
      }
    }
  }
  window.beginOffset = replayBeginOffset;
  window.editFloorOffset =
      std::max(activationBeginOffset, clampedStablePrefixFloorOffset);
  return window;
}

RecoveryWindow compute_recovery_window(const FailureSnapshot &snapshot,
                                       std::uint32_t tokenCount) noexcept {
  return compute_recovery_window(snapshot, tokenCount, tokenCount, 0);
}

namespace {

/// Per-element classification used by
/// `entry_rule_has_unguarded_leading_visible_entry` to walk the entry
/// rule's grammar shape without leaking grammar-recursion details to
/// `RecoverySearch`.
enum class LeadingVisibleEntryKind : std::uint8_t {
  None,
  Unguarded,
  PredicateGuarded,
};

[[nodiscard]] LeadingVisibleEntryKind
classify_leading_visible_entry(const grammar::AbstractElement &element) noexcept {
  using enum grammar::ElementKind;
  switch (element.getKind()) {
  case AndPredicate:
  case NotPredicate:
    return LeadingVisibleEntryKind::PredicateGuarded;
  case Assignment:
    return classify_leading_visible_entry(
        *static_cast<const grammar::Assignment &>(element).getElement());
  case ParserRule:
    return classify_leading_visible_entry(
        *static_cast<const grammar::ParserRule &>(element).getElement());
  case InfixRule:
    return classify_leading_visible_entry(
        *static_cast<const grammar::InfixRule &>(element).getElement());
  case DataTypeRule:
    return classify_leading_visible_entry(
        *static_cast<const grammar::DataTypeRule &>(element).getElement());
  case Repetition: {
    const auto &repetition = static_cast<const grammar::Repetition &>(element);
    return repetition.getMin() == 0u
               ? LeadingVisibleEntryKind::None
               : classify_leading_visible_entry(*repetition.getElement());
  }
  case Group: {
    const auto &group = static_cast<const grammar::Group &>(element);
    for (std::size_t index = 0; index < group.size(); ++index) {
      const auto *child = group.get(index);
      if (child == nullptr) {
        continue;
      }
      const auto childKind = classify_leading_visible_entry(*child);
      if (childKind != LeadingVisibleEntryKind::None) {
        return childKind;
      }
      if (!child->isNullable()) {
        return LeadingVisibleEntryKind::None;
      }
    }
    return LeadingVisibleEntryKind::None;
  }
  case OrderedChoice: {
    const auto &choice = static_cast<const grammar::OrderedChoice &>(element);
    bool sawGuarded = false;
    for (std::size_t index = 0; index < choice.size(); ++index) {
      const auto *child = choice.get(index);
      if (child == nullptr) {
        continue;
      }
      const auto childKind = classify_leading_visible_entry(*child);
      if (childKind == LeadingVisibleEntryKind::Unguarded) {
        return LeadingVisibleEntryKind::Unguarded;
      }
      sawGuarded = sawGuarded ||
                   childKind == LeadingVisibleEntryKind::PredicateGuarded;
    }
    return sawGuarded ? LeadingVisibleEntryKind::PredicateGuarded
                      : LeadingVisibleEntryKind::None;
  }
  case UnorderedGroup: {
    const auto &group = static_cast<const grammar::UnorderedGroup &>(element);
    bool sawGuarded = false;
    for (std::size_t index = 0; index < group.size(); ++index) {
      const auto *child = group.get(index);
      if (child == nullptr) {
        continue;
      }
      const auto childKind = classify_leading_visible_entry(*child);
      if (childKind == LeadingVisibleEntryKind::Unguarded) {
        return LeadingVisibleEntryKind::Unguarded;
      }
      sawGuarded = sawGuarded ||
                   childKind == LeadingVisibleEntryKind::PredicateGuarded;
    }
    return sawGuarded ? LeadingVisibleEntryKind::PredicateGuarded
                      : LeadingVisibleEntryKind::None;
  }
  case Create:
  case Nest:
    return LeadingVisibleEntryKind::None;
  case AnyCharacter:
  case CharacterRange:
  case Literal:
  case TerminalRule:
  case InfixOperator:
    return LeadingVisibleEntryKind::Unguarded;
  }
  return LeadingVisibleEntryKind::Unguarded;
}

} // namespace

bool entry_rule_has_unguarded_leading_visible_entry(
    const grammar::ParserRule &entryRule) noexcept {
  if (entryRule.getKind() != grammar::ElementKind::ParserRule) {
    return false;
  }
  return classify_leading_visible_entry(*entryRule.getElement()) !=
         LeadingVisibleEntryKind::PredicateGuarded;
}

} // namespace pegium::parser::detail
