#include <pegium/parser/RecoveryAnalysis.hpp>

#include <algorithm>
#include <utility>

#include <pegium/parser/ParseContext.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::parser::detail {

namespace {

[[nodiscard]] std::uint32_t
clamp_window_token_count(std::uint32_t tokenCount,
                         const ParseOptions &options) noexcept {
  const auto upperBound = std::max(options.recoveryWindowTokenCount,
                                   options.maxRecoveryWindowTokenCount);
  return std::min(tokenCount, upperBound);
}

template <typename ContextFactory>
[[nodiscard]] StrictParseResult
run_strict_parse_with_context(const grammar::ParserRule &entryRule,
                              const Skipper &skipper,
                              const workspace::Document &document,
                              const utils::CancellationToken &cancelToken,
                              ContextFactory &&makeContext) {
  StrictParseResult result;
  result.summary.inputSize =
      static_cast<TextOffset>(document.textView().size());

  auto cst = std::make_unique<RootCstNode>(document);
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

} // namespace

void FailureHistoryRecorder::rewind(Checkpoint checkpoint) noexcept {
  _currentVisibleLeafCount = checkpoint;
}

void FailureHistoryRecorder::onLeaf(const char *begin, const char *end,
                                    const grammar::AbstractElement *element,
                                    bool hidden) noexcept {
  if (hidden || begin == nullptr || end == nullptr || end <= begin) {
    return;
  }
  const FailureLeaf leaf{.beginOffset = static_cast<TextOffset>(begin - _inputBegin),
                         .endOffset = static_cast<TextOffset>(end - _inputBegin),
                         .element = element};
  if (_currentVisibleLeafCount < _visibleLeaves.size()) {
    _visibleLeaves[_currentVisibleLeafCount] = leaf;
  } else {
    _visibleLeaves.push_back(leaf);
  }
  ++_currentVisibleLeafCount;
  updateFurthest(end);
}

void FailureHistoryRecorder::onCursor(const char *cursor) noexcept {
  updateFurthest(cursor);
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

void FailureHistoryRecorder::updateFurthest(const char *cursor) noexcept {
  const auto offset = static_cast<TextOffset>(cursor - _inputBegin);
  if (offset < _furthestOffset) {
    return;
  }
  _furthestOffset = offset;
  _furthestVisibleLeafCount = _currentVisibleLeafCount;
}

StrictParseResult run_strict_parse(const grammar::ParserRule &entryRule,
                                   const Skipper &skipper,
                                   const workspace::Document &document,
                                   const utils::CancellationToken &cancelToken,
                                   FailureHistoryRecorder *failureRecorder) {
  if (failureRecorder == nullptr) {
    return run_strict_parse_with_context(
        entryRule, skipper, document, cancelToken,
        [](CstBuilder &builder, const Skipper &localSkipper,
           const utils::CancellationToken &localCancelToken) {
          return ParseContext{builder, localSkipper, localCancelToken};
        });
  }
  return run_strict_parse_with_context(
      entryRule, skipper, document, cancelToken,
      [failureRecorder](CstBuilder &builder, const Skipper &localSkipper,
                        const utils::CancellationToken &localCancelToken) {
        return TrackedParseContext{builder, localSkipper, *failureRecorder,
                                   localCancelToken};
      });
}

FailureAnalysisResult analyze_failure(const grammar::ParserRule &entryRule,
                                      const Skipper &skipper,
                                      const workspace::Document &document,
                                      const StrictParseSummary &strictSummary,
                                      const utils::CancellationToken &cancelToken) {
  FailureHistoryRecorder recorder(document.textView().data());
  FailureAnalysisResult result;
  result.strictResult =
      run_strict_parse(entryRule, skipper, document, cancelToken, &recorder);
  result.strictResult.summary = strictSummary;
  result.snapshot = recorder.snapshot(strictSummary.maxCursorOffset);
  return result;
}

FailureSnapshot snapshot_from_committed_cst(const RootCstNode &cst,
                                            TextOffset maxCursorOffset) noexcept {
  FailureSnapshot snapshot{.maxCursorOffset = maxCursorOffset};
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
                                       std::uint32_t tokenCount) noexcept {
  RecoveryWindow window{
      .beginOffset = snapshot.maxCursorOffset,
      .maxCursorOffset = snapshot.maxCursorOffset,
      .tokenCount = tokenCount,
  };
  if (snapshot.failureLeafHistory.empty() || tokenCount == 0 ||
      !snapshot.hasFailureToken) {
    return window;
  }

  const auto beginIndex =
      snapshot.hasFailureToken
          ? static_cast<std::uint32_t>(
                snapshot.failureTokenIndex > tokenCount
                    ? snapshot.failureTokenIndex - tokenCount
                    : 0)
          : 0u;

  window.visibleLeafBeginIndex = beginIndex;
  window.beginOffset = snapshot.failureLeafHistory[beginIndex].beginOffset;
  return window;
}

std::optional<std::uint32_t>
next_recovery_window_token_count(std::uint32_t currentTokenCount,
                                 const ParseOptions &options) noexcept {
  const auto maxTokenCount = clamp_window_token_count(
      options.maxRecoveryWindowTokenCount, options);
  if (currentTokenCount == 0 || currentTokenCount >= maxTokenCount) {
    return std::nullopt;
  }

  const auto next = std::min<std::uint32_t>(currentTokenCount * 2u,
                                            options.maxRecoveryWindowTokenCount);
  if (next <= currentTokenCount) {
    return std::nullopt;
  }
  return next;
}

} // namespace pegium::parser::detail
