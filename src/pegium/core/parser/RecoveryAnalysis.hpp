#pragma once

/// Failure analysis helpers used to seed parser recovery attempts.

#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium::parser::detail {

struct StrictParseSummary {
  TextOffset inputSize = 0;
  TextOffset parsedLength = 0;
  TextOffset lastVisibleCursorOffset = 0;
  TextOffset maxCursorOffset = 0;
  bool entryRuleMatched = false;
  bool fullMatch = false;
};

struct StrictParseResult {
  StrictParseSummary summary;
  std::unique_ptr<RootCstNode> cst;
};

struct FailureLeaf {
  TextOffset beginOffset = 0;
  TextOffset endOffset = 0;
  const grammar::AbstractElement *element = nullptr;
};

struct FailureSnapshot {
  TextOffset maxCursorOffset = 0;
  std::vector<FailureLeaf> failureLeafHistory;
  std::size_t failureTokenIndex = 0;
  bool hasFailureToken = false;
};

struct FailureAnalysisResult {
  StrictParseResult strictResult;
  FailureSnapshot snapshot;
};

struct RecoveryWindow {
  TextOffset beginOffset = 0;
  TextOffset maxCursorOffset = 0;
  std::uint32_t tokenCount = 0;
  std::uint32_t visibleLeafBeginIndex = 0;
};

class FailureHistoryRecorder {
public:
  using Checkpoint = std::size_t;

  explicit FailureHistoryRecorder(const char *inputBegin) noexcept
      : _inputBegin(inputBegin) {}

  [[nodiscard]] Checkpoint mark() const noexcept { return _currentVisibleLeafCount; }

  void rewind(Checkpoint checkpoint) noexcept;
  void onLeaf(const char *begin, const char *end,
              const grammar::AbstractElement *element, bool hidden) noexcept;
  void onCursor(const char *cursor) noexcept;
  [[nodiscard]] FailureSnapshot snapshot(TextOffset maxCursorOffset) const;

private:
  void updateFurthest(const char *cursor) noexcept;

  const char *_inputBegin = nullptr;
  TextOffset _furthestOffset = 0;
  std::size_t _currentVisibleLeafCount = 0;
  std::size_t _furthestVisibleLeafCount = 0;
  std::vector<FailureLeaf> _visibleLeaves;
};

[[nodiscard]] StrictParseResult
run_strict_parse(const grammar::ParserRule &entryRule, const Skipper &skipper,
                 const text::TextSnapshot &text,
                 const utils::CancellationToken &cancelToken = {},
                 FailureHistoryRecorder *failureRecorder = nullptr);

[[nodiscard]] FailureAnalysisResult
analyze_failure(const grammar::ParserRule &entryRule, const Skipper &skipper,
                const text::TextSnapshot &text,
                const StrictParseSummary &strictSummary,
                const utils::CancellationToken &cancelToken = {});

[[nodiscard]] FailureSnapshot
snapshot_from_committed_cst(const RootCstNode &cst,
                            TextOffset maxCursorOffset) noexcept;

void finalize_failure_token_index(FailureSnapshot &snapshot) noexcept;

[[nodiscard]] RecoveryWindow
compute_recovery_window(const FailureSnapshot &snapshot,
                        std::uint32_t tokenCount) noexcept;

[[nodiscard]] std::optional<std::uint32_t>
next_recovery_window_token_count(std::uint32_t currentTokenCount,
                                 const ParseOptions &options) noexcept;

} // namespace pegium::parser::detail
