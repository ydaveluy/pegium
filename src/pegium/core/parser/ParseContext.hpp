#pragma once

/// Strict, tracked, and recovery parse contexts used while building the CST.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <type_traits>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/parser/ChoiceAttempt.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/utils/TextUtils.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <utility>
#include <vector>

namespace pegium::parser {

struct ParseContext {
  struct Checkpoint {
    const char *cursor;
    const char *lastVisibleCursor;
    CstBuilder::Checkpoint builder;
  };

  const char *const begin;
  const char *const end;

  ParseContext(
      CstBuilder &builder, const Skipper &skipper,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token)
      noexcept
      : begin(builder.input_begin()), end(builder.input_end()), _cursor(begin),
        _lastVisibleCursor(begin), _builder(builder), _skipper(&skipper),
        _cancelToken(cancelToken) {
  }

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextMark);

    return {.cursor = _cursor,
            .lastVisibleCursor = _lastVisibleCursor,
            .builder = _builder.mark()};
  }

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextRewind);
    _builder.rewind(checkpoint.builder);
    _cursor = checkpoint.cursor;
    _lastVisibleCursor = checkpoint.lastVisibleCursor;
  }

  using SkipperGuard = detail::SkipperGuard<ParseContext>;

  [[nodiscard]] SkipperGuard
  with_skipper(const Skipper &overrideSkipper) noexcept {
    return SkipperGuard{*this, _skipper, overrideSkipper};
  }

  [[nodiscard]] inline const char *enter() {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextEnter);
    _builder.enter();

    return _cursor;
  }

  inline void exit(const char *checkpoint,
                   const grammar::AbstractElement *element) {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextExit);
    _builder.exit(static_cast<TextOffset>(checkpoint - begin),
                  lastVisibleCursorOffset(), element);
    utils::throw_if_cancelled(_cancelToken);
  }

  [[nodiscard]] inline NodeCount node_count() const noexcept {
    return _builder.node_count();
  }

  inline void
  override_grammar_element(NodeId id,
                           const grammar::AbstractElement *element) noexcept {
    _builder.override_grammar_element(id, element);
  }

  [[nodiscard]] constexpr const char *cursor() const noexcept {
    return _cursor;
  }
  [[nodiscard]] constexpr TextOffset cursorOffset() const noexcept {
    return static_cast<TextOffset>(_cursor - begin);
  }

  [[nodiscard]] constexpr TextOffset lastVisibleCursorOffset() const noexcept {
    return static_cast<TextOffset>(_lastVisibleCursor - begin);
  }

  /// Strict-only `ParseContext` does not track a max cursor distinct from
  /// the live cursor — backtracking does not produce observable failure
  /// state on this layer. Returns the live cursor so external callers
  /// (tests and recovery summaries that may receive either context type)
  /// see a coherent value. The real tracking lives in
  /// `TrackedParseContext`, which shadows this accessor.
  [[nodiscard]] constexpr const char *maxCursor() const noexcept {
    return _cursor;
  }

  [[nodiscard]] constexpr TextOffset maxCursorOffset() const noexcept {
    return static_cast<TextOffset>(_cursor - begin);
  }

  [[nodiscard]] constexpr const utils::CancellationToken &
  cancellationToken() const noexcept {
    return _cancelToken;
  }
  inline void skip() noexcept {
    _cursor = _skipper->skip(_cursor, _builder);
  }

  inline void leaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false) {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextLeaf);
    const auto beginOffset = cursorOffset();
    const auto endOffset = static_cast<TextOffset>(endPtr - begin);
    _builder.leaf(beginOffset, endOffset, element, hidden, recovered);
    _cursor = endPtr;
    if (!hidden) [[likely]] {
      _lastVisibleCursor = endPtr;
    }
  }

  inline void leaf(const char *beginPtr, const char *endPtr,
                   const grammar::AbstractElement *element, bool hidden,
                   bool recovered) {
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextLeaf);
    const auto beginOffset = static_cast<TextOffset>(beginPtr - begin);
    const auto endOffset = static_cast<TextOffset>(endPtr - begin);
    _builder.leaf(beginOffset, endOffset, element, hidden, recovered);
    _cursor = endPtr;
    if (!hidden) [[likely]] {
      _lastVisibleCursor = endPtr;
    }
  }
  protected:
  const char *_cursor;
  const char *_lastVisibleCursor;
  CstBuilder &_builder;
  const Skipper *_skipper;
  const utils::CancellationToken &_cancelToken;
};

struct RecoveryContext;

[[nodiscard]] inline constexpr bool elements_equivalent_for_replay(
    const grammar::AbstractElement *lhs,
    const grammar::AbstractElement *rhs) noexcept {
  if (lhs == rhs) {
    return true;
  }
  if (lhs == nullptr || rhs == nullptr) {
    return false;
  }
  if (lhs->getKind() != rhs->getKind()) {
    return false;
  }
  if (lhs->getKind() == grammar::ElementKind::Literal) {
    const auto *lhsLit = static_cast<const grammar::Literal *>(lhs);
    const auto *rhsLit = static_cast<const grammar::Literal *>(rhs);
    return lhsLit->getValue() == rhsLit->getValue() &&
           lhsLit->isCaseSensitive() == rhsLit->isCaseSensitive();
  }
  return false;
}

// Strict parse context with failure-history tracking.
//
// This is not the nominal hot-path context: plain strict parses use
// `ParseContext`. `TrackedParseContext` exists for two internal cases only:
// - strict failure analysis with a `FailureHistoryRecorder`
// - the base class of `RecoveryContext`
struct TrackedParseContext : ParseContext {
  struct Checkpoint {
    ParseContext::Checkpoint parseCheckpoint;
    std::size_t failureHistorySize;
    bool recordFailureHistory;
  };

  TrackedParseContext(
      CstBuilder &builder, const Skipper &skipper,
      detail::FailureHistoryRecorder &failureRecorder,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) noexcept
      : ParseContext(builder, skipper, cancelToken), _maxCursor(begin),
        _failureRecorder(failureRecorder) {}

  [[nodiscard]] constexpr const char *maxCursor() const noexcept {
    return _maxCursor;
  }

  constexpr void restoreMaxCursor(const char *cursor) noexcept {
    _maxCursor = cursor;
  }

  /// Monotonically raise `_maxCursor` to replay the cumulative side effect
  /// of a memoized exploration that was skipped on cache hit.
  constexpr void bumpMaxCursor(const char *cursor) noexcept {
    if (cursor > _maxCursor) {
      _maxCursor = cursor;
    }
  }

  [[nodiscard]] constexpr TextOffset maxCursorOffset() const noexcept {
    return static_cast<TextOffset>(_maxCursor - begin);
  }

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    return {.parseCheckpoint = ParseContext::mark(),
            .failureHistorySize =
                _recordFailureHistory ? _failureRecorder.mark() : 0u,
            .recordFailureHistory = _recordFailureHistory};
  }

  using ParseContext::rewind;

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    ParseContext::rewind(checkpoint.parseCheckpoint);
    _recordFailureHistory = checkpoint.recordFailureHistory;
    if (_recordFailureHistory) [[likely]] {
      _failureRecorder.rewind(checkpoint.failureHistorySize);
    }
  }

  inline void skip() noexcept;

  inline void leaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false);
  inline void leaf(const char *beginPtr, const char *endPtr,
                   const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false);

  [[nodiscard]] SkipperGuard
  with_skipper(const Skipper &overrideSkipper) noexcept {
    // Invalidate the no-builder skip cache: a different skipper can map the
    // same cursor to a different end.
    _skipCacheBegin = nullptr;
    return SkipperGuard{*this, _skipper, overrideSkipper};
  }

  [[nodiscard]] const char *skip_without_builder(const char *begin) const noexcept {
    // 1-entry cache: the no-builder skipper is purely a function of (begin,
    // _skipper) and is called heavily during recovery exploration at
    // overlapping positions. Caching the most recent (begin -> end) result
    // avoids re-walking the same trivia run dozens of times per offset.
    if (begin == _skipCacheBegin) {
      return _skipCacheEnd;
    }
    const char *const end = _skipper->skip(begin);
    _skipCacheBegin = begin;
    _skipCacheEnd = end;
    return end;
  }

  [[nodiscard]] constexpr bool isFailureHistoryRecordingEnabled() const noexcept {
    return _recordFailureHistory;
  }

  [[nodiscard]] std::size_t failureHistorySize() const noexcept {
    return _failureRecorder.mark();
  }

  [[nodiscard]] std::size_t furthestFailureHistorySize() const noexcept {
    return _failureRecorder.furthestVisibleLeafCount();
  }

  void bumpFurthestFailureHistorySize(std::size_t value) noexcept {
    _failureRecorder.bumpFurthestVisibleLeafCount(value);
  }

  void bumpFurthestFailureOffset(TextOffset value) noexcept {
    _failureRecorder.bumpFurthestOffset(value);
  }

  [[nodiscard]] TextOffset furthestFailureOffset() const noexcept {
    return _failureRecorder.furthestOffset();
  }

protected:
  /// Highest cursor position reached during parsing, including failed
  /// alternatives that were rewound. Recovery uses this to position the
  /// failure window. Strict-only `ParseContext` does not track it; this
  /// state lives at the `TrackedParseContext` layer where recovery becomes
  /// observable.
  const char *_maxCursor;
  detail::FailureHistoryRecorder &_failureRecorder;
  // 1-entry no-builder skip cache. `mutable` so const callers can populate
  // it without compromising the logical const-ness of `skip_without_builder`.
  // Strict-only `ParseContext` does not need a no-builder skipper (no recovery
  // probes call into it), so the cache and its companion `skip_without_builder`
  // live here.
  mutable const char *_skipCacheBegin = nullptr;
  mutable const char *_skipCacheEnd = nullptr;
  bool _recordFailureHistory = true;
  bool _runRecoveryBookkeeping = false;
};

struct RecoveryContext : TrackedParseContext {
  struct EditWindow {
    TextOffset beginOffset = 0;
    TextOffset editFloorOffset = 0;
    TextOffset maxCursorOffset = 0;
    std::uint32_t forwardTokenCount = 0;
    std::uint32_t replayForwardTokenCount = 1;
  };

  using RecoveryEdit = detail::SyntaxScriptEntry;

  struct EditBudgetState {
    std::uint32_t consecutiveDeletes = 0;
    std::uint32_t editCost = 0;
    std::uint32_t editCount = 0;
    bool hadEdits = false;
    bool allowBudgetOverflowEdits = false;
  };

  struct WindowReplayState {
    std::uint32_t activeWindowEditCostBase = 0;
    std::uint32_t activeWindowEditCountBase = 0;
    std::uint32_t currentForwardVisibleLeafCount = 0;
    std::uint32_t strictVisibleLeafCountAfterRecovery = 0;
    std::uint32_t completedRecoveryWindows = 0;
    bool inRecoveryPhase = true;
    bool reachedRecoveryTarget = false;
    bool stableAfterRecovery = false;
    bool awaitingStrictStability = false;
    bool recoveryBookkeepingEnabled = true;
    bool activeEditWindowCompleted = false;
  };

  struct DeleteBridgeState {
    const char *pendingHiddenTriviaStart = nullptr;
    const char *pendingHiddenTriviaEnd = nullptr;
  };

  struct RecoveryState {
    EditBudgetState editBudget{};
    WindowReplayState windowReplay{};
    bool frontierBlocked = false;
  };

  static_assert(std::is_trivially_copyable_v<EditBudgetState>);
  static_assert(std::is_trivially_copyable_v<WindowReplayState>);
  static_assert(std::is_trivially_copyable_v<DeleteBridgeState>);
  static_assert(std::is_trivially_copyable_v<RecoveryState>);

  struct Checkpoint {
    TrackedParseContext::Checkpoint parseCheckpoint;
    RecoveryState recoveryState;
    std::uint32_t recoveryEditCount;
    std::uint32_t committedRecoveryEditIndex;
    std::uint32_t editWindowReplayForwardTokenCount;
    DeleteBridgeState deleteBridge;
  };

  using EditStateGuard = detail::EditStateGuard<RecoveryContext>;
  using ActiveRecoveryGuard = detail::ActiveRecoveryStack::Guard<RecoveryContext>;
  RecoveryContext(
      CstBuilder &builder, const Skipper &skipper,
      detail::FailureHistoryRecorder &failureRecorder,
      const utils::CancellationToken &cancelToken =
          utils::default_cancel_token) noexcept
      : TrackedParseContext(builder, skipper, failureRecorder, cancelToken) {
    _runRecoveryBookkeeping = true;
  }

  bool allowInsert = true;
  bool allowDelete = true;
  bool allowExtendedDeleteScan = true;
  bool allowDeleteRetry = true;
  bool skipAfterDelete = true;
  bool trackEditState = true;
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;
  std::uint32_t stabilityTokenCount = 2;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxResyncSkipBytes = 4096;
  /// Hard cap on the cumulative number of `ParserRule` recovery entries
  /// during a single recovery window. A pathological grammar shape (e.g.
  /// unclosed nested call expressions) can drive `evaluate_editable_recovery_candidate`
  /// into an exponentially-branching tree of speculative parses; counting
  /// every entry and aborting once the cap is hit collapses that exploration
  /// so the outer driver falls back to a less ambitious candidate. Reset to
  /// 0 implicitly because every `try_recovery_window` creates a fresh
  /// `RecoveryContext`.
  std::uint32_t maxRecoveryRuleEntries = 12000;
  std::uint32_t recoveryRuleEntries = 0;
  TextOffset editFloorOffset = 0;
  std::optional<EditWindow> editWindow;
  bool allowTopLevelPartialSuccess = false;
  bool allowProvisionalFuzzyReplace = false;
  TextOffset provisionalFuzzyReplaceAnchorOffset = 0;
  std::vector<RecoveryEdit> recoveryEdits;
  std::vector<RecoveryEdit> committedRecoveryEdits;
  std::uint32_t committedRecoveryEditIndex = 0;
  TextOffset committedRecoveryResumeFloor = 0;
  RecoveryState recoveryState;
  bool allowDestructiveWindowContinuation = false;
  bool allowLeadingTerminalInsertScope = false;
  DeleteBridgeState _deleteBridge{};

  detail::ChoiceRecoverCache choiceRecoverCache{};

  /// Per-parse cache memoizing fuzzy-keyword DP results. Lives on the
  /// recovery context so its lifetime tracks one parse exactly: stable
  /// pointers (grammar literal storage, immutable text snapshot) make
  /// identity-based lookup safe within that scope, and resetting the parse
  /// context clears the cache for free. See
  /// `LiteralFuzzyCandidatesCache` documentation for why this must not be
  /// promoted to a global or thread-local.
  detail::LiteralFuzzyCandidatesCache literalFuzzyCandidatesCache{};

  using FollowProbeFn = bool (*)(RecoveryContext &, const void *);
  FollowProbeFn _followProbeFn = nullptr;
  const void *_followProbeData = nullptr;
  FollowProbeFn _recoverableFollowProbeFn = nullptr;
  const void *_recoverableFollowProbeData = nullptr;
  FollowProbeFn _recoverableFollowConsumesVisibleProbeFn = nullptr;
  const void *_recoverableFollowConsumesVisibleProbeData = nullptr;

  // Returns true when the immediately-enclosing Group's successor element would
  // accept the current cursor. Grammar-agnostic: relies only on structural
  // probing installed by Group around each element's parse.
  [[nodiscard]] bool probeFollowAcceptsHere() noexcept {
    if (_followProbeFn == nullptr) {
      return false;
    }
    return _followProbeFn(*this, _followProbeData);
  }

  // Returns true when the immediately-enclosing Group's successor can recover
  // locally from the current cursor, even if it does not accept strictly yet.
  [[nodiscard]] bool probeRecoverableFollowHere() noexcept {
    if (_recoverableFollowProbeFn == nullptr) {
      return false;
    }
    return _recoverableFollowProbeFn(*this, _recoverableFollowProbeData);
  }

  // Stronger recoverable-follow probe: true only when the enclosing suffix can
  // recover from this cursor and commit visible source after the recovery site.
  // Repetition uses this to avoid stealing a parent boundary with a synthetic
  // local separator when the parent can prove real continuation.
  [[nodiscard]] bool probeRecoverableFollowConsumesVisibleHere() noexcept {
    if (_recoverableFollowConsumesVisibleProbeFn == nullptr) {
      return false;
    }
    return _recoverableFollowConsumesVisibleProbeFn(
        *this, _recoverableFollowConsumesVisibleProbeData);
  }

  // Two install modes for the follow probe guard:
  //   - Local: the new probe replaces the outer probe; queries do not
  //     chain. Use for non-last Group elements where the immediate
  //     successor is the only valid follow (chaining to the OUTER
  //     would let outer-scope continuations mask in-group failures).
  //   - PassThroughOuter: the new probe is null; queries fall through
  //     to the previously-installed (outer) probe. Use for the LAST
  //     element of a Group, where the semantic follow is the OUTER
  //     scope's follow — without this chain, the Group's tail-probe
  //     returns "past end -> false" and a repeated tail nested inside
  //     an optional parent cannot see the parent's next structural
  //     element, cascading into a panic-mode delete.
  enum class FollowProbeMode : std::uint8_t {
    Local,
    PassThroughOuter,
  };

  struct [[nodiscard]] FollowProbeGuard {
    FollowProbeGuard(RecoveryContext &c, FollowProbeFn fn, const void *data,
                     FollowProbeFn recoverableFn = nullptr,
                     const void *recoverableData = nullptr,
                     FollowProbeFn recoverableConsumesVisibleFn = nullptr,
                     const void *recoverableConsumesVisibleData = nullptr,
                     FollowProbeMode mode = FollowProbeMode::Local) noexcept
        : ctx(&c), prevFn(c._followProbeFn),
          prevData(c._followProbeData),
          prevRecoverableFn(c._recoverableFollowProbeFn),
          prevRecoverableData(c._recoverableFollowProbeData),
          prevRecoverableConsumesVisibleFn(
              c._recoverableFollowConsumesVisibleProbeFn),
          prevRecoverableConsumesVisibleData(
              c._recoverableFollowConsumesVisibleProbeData) {
      if (mode == FollowProbeMode::PassThroughOuter) {
        // Pass-through: queries skip the (always-false) local probe
        // and go straight to the outer one. The newFn slot is unused.
        c._followProbeFn =
            prevFn == nullptr ? nullptr : &passThroughStrictWrapper;
        c._followProbeData = prevFn == nullptr ? nullptr : this;
        c._recoverableFollowProbeFn = prevRecoverableFn == nullptr
                                          ? nullptr
                                          : &passThroughRecoverableWrapper;
        c._recoverableFollowProbeData =
            prevRecoverableFn == nullptr ? nullptr : this;
        c._recoverableFollowConsumesVisibleProbeFn =
            prevRecoverableConsumesVisibleFn == nullptr
                ? nullptr
                : &passThroughRecoverableConsumesVisibleWrapper;
        c._recoverableFollowConsumesVisibleProbeData =
            prevRecoverableConsumesVisibleFn == nullptr ? nullptr : this;
      } else {
        c._followProbeFn = fn;
        c._followProbeData = data;
        c._recoverableFollowProbeFn = recoverableFn;
        c._recoverableFollowProbeData = recoverableData;
        c._recoverableFollowConsumesVisibleProbeFn =
            recoverableConsumesVisibleFn;
        c._recoverableFollowConsumesVisibleProbeData =
            recoverableConsumesVisibleData;
      }
    }
    FollowProbeGuard(const FollowProbeGuard &) = delete;
    FollowProbeGuard &operator=(const FollowProbeGuard &) = delete;
    FollowProbeGuard(FollowProbeGuard &&) = delete;
    FollowProbeGuard &operator=(FollowProbeGuard &&) = delete;
    ~FollowProbeGuard() noexcept {
      if (ctx) {
        ctx->_followProbeFn = prevFn;
        ctx->_followProbeData = prevData;
        ctx->_recoverableFollowProbeFn = prevRecoverableFn;
        ctx->_recoverableFollowProbeData = prevRecoverableData;
        ctx->_recoverableFollowConsumesVisibleProbeFn =
            prevRecoverableConsumesVisibleFn;
        ctx->_recoverableFollowConsumesVisibleProbeData =
            prevRecoverableConsumesVisibleData;
      }
    }

  private:
    static bool passThroughStrictWrapper(RecoveryContext &c,
                                         const void *data) noexcept {
      const auto *self = static_cast<const FollowProbeGuard *>(data);
      return self->prevFn != nullptr && self->prevFn(c, self->prevData);
    }

    static bool passThroughRecoverableWrapper(RecoveryContext &c,
                                              const void *data) noexcept {
      const auto *self = static_cast<const FollowProbeGuard *>(data);
      return self->prevRecoverableFn != nullptr &&
             self->prevRecoverableFn(c, self->prevRecoverableData);
    }

    static bool
    passThroughRecoverableConsumesVisibleWrapper(RecoveryContext &c,
                                                 const void *data) noexcept {
      const auto *self = static_cast<const FollowProbeGuard *>(data);
      return self->prevRecoverableConsumesVisibleFn != nullptr &&
             self->prevRecoverableConsumesVisibleFn(
                 c, self->prevRecoverableConsumesVisibleData);
    }

    RecoveryContext *ctx;
    FollowProbeFn prevFn;
    const void *prevData;
    FollowProbeFn prevRecoverableFn;
    const void *prevRecoverableData;
    FollowProbeFn prevRecoverableConsumesVisibleFn;
    const void *prevRecoverableConsumesVisibleData;
  };

  [[nodiscard]] FollowProbeGuard
  withFollowProbe(FollowProbeFn fn, const void *data,
                  FollowProbeFn recoverableFn = nullptr,
                  const void *recoverableData = nullptr,
                  FollowProbeFn recoverableConsumesVisibleFn = nullptr,
                  const void *recoverableConsumesVisibleData = nullptr,
                  FollowProbeMode mode = FollowProbeMode::Local) noexcept {
    return FollowProbeGuard{*this,
                            fn,
                            data,
                            recoverableFn,
                            recoverableData,
                            recoverableConsumesVisibleFn,
                            recoverableConsumesVisibleData,
                            mode};
  }

  [[nodiscard]] FollowProbeGuard
  withPassThroughOuterFollowProbe() noexcept {
    return FollowProbeGuard{
        *this,   nullptr, nullptr, nullptr,
        nullptr, nullptr, nullptr, FollowProbeMode::PassThroughOuter};
  }

  [[nodiscard]] constexpr const char *furthestExploredCursor() const noexcept {
    return maxCursor();
  }

  [[nodiscard]] constexpr TextOffset furthestExploredOffset() const noexcept {
    return maxCursorOffset();
  }

  constexpr void restoreFurthestExploredCursor(const char *cursor) noexcept {
    restoreMaxCursor(cursor);
  }

  /// Monotonic restore: only lift the furthest-explored cursor back up to
  /// `cursor` when the current value has fallen below it (e.g. after a
  /// rewind). Work that legitimately progressed the frontier further is
  /// preserved.
  constexpr void
  bumpFurthestExploredCursor(const char *cursor) noexcept {
    if (cursor > maxCursor()) {
      restoreMaxCursor(cursor);
    }
  }

  inline void refreshRecoveryPhase() noexcept {
    auto &windowReplay = recoveryState.windowReplay;
    if (!windowReplay.recoveryBookkeepingEnabled) [[likely]] {
      return;
    }
    refreshRecoveryPhaseSlow();
  }

private:
  void refreshRecoveryPhaseSlow() noexcept;

public:

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    return {.parseCheckpoint = TrackedParseContext::mark(),
            .recoveryState = recoveryState,
            .recoveryEditCount =
                static_cast<std::uint32_t>(recoveryEdits.size()),
            .committedRecoveryEditIndex = committedRecoveryEditIndex,
            .editWindowReplayForwardTokenCount =
                editWindow.has_value() ? editWindow->replayForwardTokenCount
                                       : 0u,
            .deleteBridge = _deleteBridge};
  }

  using TrackedParseContext::rewind;

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    TrackedParseContext::rewind(checkpoint.parseCheckpoint);
    _deleteBridge = checkpoint.deleteBridge;
    committedRecoveryEditIndex = checkpoint.committedRecoveryEditIndex;
    if (editWindow.has_value()) [[likely]] {
      editWindow->replayForwardTokenCount =
          checkpoint.editWindowReplayForwardTokenCount;
    }
    recoveryState = checkpoint.recoveryState;
    // `recoveryRuleEntries` is intentionally NOT rewound: the cap is a
    // per-window global budget on speculative ParserRule entries, and a
    // rewind to a sibling branch must continue to see the total work
    // already consumed. Otherwise sibling branches would each get fresh
    // budget and the sum would explode exponentially on pathological
    // grammar shapes (the very thing the cap exists to bound).
    // Speculative parses that didn't push an edit dominate the rewind path;
    // skip the resize call (and the underlying destructor loop / size-store
    // sequence) when the count is unchanged.
    if (recoveryEdits.size() != checkpoint.recoveryEditCount) [[unlikely]] {
      recoveryEdits.resize(checkpoint.recoveryEditCount);
    }
  }

  inline void exit(const char *checkpoint,
                   const grammar::AbstractElement *element) {
    TrackedParseContext::exit(checkpoint, element);
    if (trackEditState && recoveryState.editBudget.consecutiveDeletes != 0)
        [[unlikely]] {
      recoveryState.editBudget.consecutiveDeletes = 0;
    }
  }

  [[nodiscard]] constexpr bool frontierBlocked() const noexcept {
    return recoveryState.frontierBlocked;
  }

  void clearFrontierBlock() noexcept {
    if (!recoveryState.frontierBlocked) {
      return;
    }
    recoveryState.frontierBlocked = false;
  }

  void blockFrontier() noexcept {
    if (recoveryState.frontierBlocked) {
      return;
    }
    recoveryState.frontierBlocked = true;
  }

  void setEditWindow(std::optional<EditWindow> window) noexcept {
    editWindow = std::move(window);
    recoveryState.windowReplay = WindowReplayState{
        .activeWindowEditCostBase = recoveryState.editBudget.editCost,
        .activeWindowEditCountBase = recoveryState.editBudget.editCount,
        .inRecoveryPhase = false,
        .recoveryBookkeepingEnabled = editWindow.has_value(),
    };
    _recordFailureHistory = recoveryState.windowReplay.recoveryBookkeepingEnabled;
    editFloorOffset = editWindow.has_value() ? editWindow->editFloorOffset
                                             : TextOffset{0};
    _deleteBridge = {};
  }

  void setCommittedRecoveryPrefix(std::vector<RecoveryEdit> edits,
                                  TextOffset resumeFloor) noexcept {
    committedRecoveryEdits = std::move(edits);
    committedRecoveryEditIndex = 0;
    committedRecoveryResumeFloor = resumeFloor;
  }

  inline void finalizeRecoveryAtEof() noexcept {
    if (cursor() != end) {
      return;
    }
    if (recoveryState.windowReplay.inRecoveryPhase &&
        currentEditWindow() != nullptr) {
      completeActiveRecoveryWindow(true);
      return;
    }
    if (recoveryState.windowReplay.awaitingStrictStability) {
      recoveryState.windowReplay.stableAfterRecovery = true;
      recoveryState.windowReplay.awaitingStrictStability = false;
      maybeDisableRecoveryBookkeeping();
    }
  }
  [[nodiscard]] constexpr bool isInRecoveryPhase() const noexcept {
    return recoveryState.windowReplay.inRecoveryPhase;
  }
  [[nodiscard]] constexpr bool hasPendingRecoveryWindows() const noexcept {
    return editWindow.has_value() &&
           !recoveryState.windowReplay.activeEditWindowCompleted;
  }

  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowBeginOffset() const noexcept {
    return hasPendingRecoveryWindows()
               ? std::max(editWindow->beginOffset, committedRecoveryResumeFloor)
               : 0;
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowActivationOffset() const noexcept {
    if (!hasPendingRecoveryWindows()) {
      return 0;
    }
    return pendingRecoveryWindowBeginOffset();
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowMaxCursorOffset() const noexcept {
    return hasPendingRecoveryWindows() ? editWindow->maxCursorOffset : 0;
  }
  [[nodiscard]] std::size_t activeRecoveryDepth() const noexcept {
    return _activeRecoveries.size();
  }
  [[nodiscard]] constexpr bool hasHadEdits() const noexcept {
    return recoveryState.editBudget.hadEdits;
  }
  [[nodiscard]] constexpr bool hasPendingCommittedRecoveryEdits() const
      noexcept {
    return committedRecoveryEditIndex < committedRecoveryEdits.size();
  }
  [[nodiscard]] constexpr bool hasPendingCommittedRecoveryEditWithin(
      TextOffset beginOffset, TextOffset endOffset) const noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    if (entry == nullptr || endOffset < beginOffset) {
      return false;
    }
    return entry->beginOffset <= endOffset && entry->endOffset >= beginOffset;
  }
  [[nodiscard]] constexpr std::uint32_t
  completedRecoveryWindowCount() const noexcept {
    return recoveryState.windowReplay.completedRecoveryWindows;
  }
  [[nodiscard]] constexpr bool hasReachedRecoveryTarget() const noexcept {
    return recoveryState.windowReplay.reachedRecoveryTarget;
  }
  [[nodiscard]] constexpr bool isStableAfterRecovery() const noexcept {
    return recoveryState.windowReplay.stableAfterRecovery;
  }

  [[nodiscard]] bool isActiveRecovery(
      const grammar::AbstractElement *element) const noexcept {
    return _activeRecoveries.contains(cursor(), element);
  }

  [[nodiscard]] ActiveRecoveryGuard
  enterActiveRecovery(const grammar::AbstractElement *element) noexcept {
    return ActiveRecoveryGuard(_activeRecoveries, *this, element);
  }

  [[nodiscard]] EditStateGuard
  withEditPermissions(bool nextAllowInsert, bool nextAllowDelete) noexcept {
    return detail::make_edit_permissions_guard(*this, nextAllowInsert,
                                               nextAllowDelete);
  }

  /// Disable insert/delete and edit-state tracking together for the scope
  /// of the returned RAII guard. Used by recovery probes and no-edit
  /// replay attempts that must observe the strict-parse view of the
  /// context without committing edits or bumping any edit-budget
  /// counters.
  [[nodiscard]] EditStateGuard withEditTrackingDisabled() noexcept {
    return detail::make_edit_state_guard(*this, false, false, false);
  }

  [[nodiscard]] constexpr bool
  allowsScopedLeadingTerminalInsertRecovery() const noexcept {
    return allowLeadingTerminalInsertScope;
  }

  [[nodiscard]] constexpr bool
  allowsCompletedWindowContinuationRecovery() const noexcept {
    return editWindow.has_value() && allowDestructiveWindowContinuation &&
           recoveryState.editBudget.hadEdits &&
           recoveryState.windowReplay.activeEditWindowCompleted &&
           !hasPendingCommittedRecoveryEdits();
  }

  [[nodiscard]] bool
  allowsCompletedWindowInsertionClusterAtCursor() const noexcept {
    return editWindow.has_value() && recoveryState.editBudget.hadEdits &&
           recoveryState.windowReplay.activeEditWindowCompleted &&
           !hasPendingCommittedRecoveryEdits() &&
           cursorOffset() >= editWindow->editFloorOffset &&
           continues_local_edit_cluster_at_cursor();
  }

  [[nodiscard]] constexpr bool canEditAtOffset(TextOffset offset) const noexcept {
    if (hasPendingCommittedRecoveryEdits()) {
      return canReplayCommittedEditAtOffset(offset);
    }
    if (!editWindow.has_value()) {
      return recoveryState.windowReplay.inRecoveryPhase &&
             offset >= editFloorOffset;
    }
    if (!hasPendingRecoveryWindows()) {
      return allowsCompletedWindowContinuationRecovery() &&
             offset >= editWindow->editFloorOffset;
    }
    return offset >= editWindow->editFloorOffset;
  }

  [[nodiscard]] constexpr bool canEdit() const noexcept {
    return canEditAtOffset(cursorOffset());
  }


  [[nodiscard]] constexpr bool canInsert() const noexcept {
    return detail::can_insert(allowInsert, canEdit());
  }

  [[nodiscard]] constexpr bool canDelete() const noexcept {
    if (_cursor >= end) {
      return false;
    }
    // A pending committed delete at the cursor must remain replayable even
    // when the active window's editFloor would otherwise refuse new edits:
    // committed edits were already accepted by an earlier window and replay
    // them is not a fresh edit. Without this every recovery decision point
    // upstream of `deleteOneCodepoint` (legality checks, scans, family
    // selection) would refuse to engage at offsets below editFloor, and the
    // committed prefix could not be threaded across windows.
    if (matching_committed_delete(cursorOffset()) != nullptr) {
      return true;
    }
    return detail::can_delete(allowDelete, canEdit(),
                              recoveryState.editBudget.consecutiveDeletes,
                              maxConsecutiveCodepointDeletes, *cursor());
  }

  [[nodiscard]] constexpr std::uint32_t currentEditCost() const noexcept {
    return recoveryState.editBudget.editCost;
  }
  [[nodiscard]] constexpr std::uint32_t currentEditCount() const noexcept {
    return recoveryState.editBudget.editCount;
  }
  [[nodiscard]] constexpr std::uint32_t
  currentWindowEditCost() const noexcept {
    return recoveryState.editBudget.editCost -
           recoveryState.windowReplay.activeWindowEditCostBase;
  }
  [[nodiscard]] constexpr std::uint32_t
  currentWindowEditCount() const noexcept {
    return recoveryState.editBudget.editCount -
           recoveryState.windowReplay.activeWindowEditCountBase;
  }

  [[nodiscard]] constexpr std::uint32_t
  editCostDelta(const Checkpoint &checkpoint) const noexcept {
    return recoveryState.editBudget.editCost -
           checkpoint.recoveryState.editBudget.editCost;
  }

  [[nodiscard]] std::size_t recoveryEditCount() const noexcept {
    return recoveryEdits.size();
  }

  [[nodiscard]] std::span<const RecoveryEdit> recoveryEditsView() const
      noexcept {
    return recoveryEdits;
  }

  [[nodiscard]] std::vector<RecoveryEdit> snapshotRecoveryEdits() const {
    return recoveryEdits;
  }

  [[nodiscard]] std::vector<RecoveryEdit> takeRecoveryEdits() noexcept {
    return std::move(recoveryEdits);
  }

  [[nodiscard]] std::vector<std::uint32_t>
  replayForwardTokenCounts() const {
    std::vector<std::uint32_t> replayCounts;
    if (editWindow.has_value()) {
      replayCounts.push_back(
          std::max(editWindow->forwardTokenCount,
                   editWindow->replayForwardTokenCount));
    }
    return replayCounts;
  }

  [[nodiscard]] constexpr bool
  canAffordEdit(ParseDiagnosticKind kind) const noexcept {
    if (recoveryState.editBudget.allowBudgetOverflowEdits) {
      return true;
    }
    if (recoveryState.windowReplay.inRecoveryPhase &&
        currentEditWindow() != nullptr) {
      return detail::can_afford_edit(currentWindowEditCount(),
                                     currentWindowEditCost(),
                                     maxEditsPerAttempt, maxEditCost, kind);
    }
    return detail::can_afford_edit(recoveryState.editBudget.editCount,
                                   recoveryState.editBudget.editCost,
                                   maxEditsPerAttempt, maxEditCost, kind);
  }
  [[nodiscard]] constexpr bool
  canAffordEdit(std::uint32_t customCost) const noexcept {
    if (recoveryState.editBudget.allowBudgetOverflowEdits) {
      return true;
    }
    if (recoveryState.windowReplay.inRecoveryPhase &&
        currentEditWindow() != nullptr) {
      return detail::can_afford_edit(currentWindowEditCount(),
                                     currentWindowEditCost(),
                                     maxEditsPerAttempt, maxEditCost,
                                     customCost);
    }
    return detail::can_afford_edit(recoveryState.editBudget.editCount,
                                   recoveryState.editBudget.editCost,
                                   maxEditsPerAttempt, maxEditCost,
                                   customCost);
  }

  inline void allowBudgetOverflowEdits() noexcept {
    if (!recoveryState.editBudget.allowBudgetOverflowEdits) {
      recoveryState.editBudget.allowBudgetOverflowEdits = true;
    }
  }

  bool insertSynthetic(const grammar::AbstractElement *element);

  bool insertSyntheticGapAt(const char *position,
                            const char *message = nullptr);

  bool deleteOneCodepoint() noexcept;

  bool extendLastDeleteThroughHiddenTrivia() noexcept;

  bool replaceLeaf(const char *endPtr, const grammar::AbstractElement *element,
                   std::uint32_t replacementCost, bool hidden = false) {
    if (!trackEditState) {
      return false;
    }
    clearPendingDeleteHiddenTriviaBridge();
    if (endPtr <= cursor() || endPtr > end) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    const auto endOffset = static_cast<TextOffset>(endPtr - begin);
    const bool replayingCommittedReplace =
        matches_committed_replace(cursorOffset(), endOffset, element);
    if (hasPendingCommittedRecoveryEdits() && !replayingCommittedReplace) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked pending committed offset=",
                            cursorOffset(), " floor=", editFloorOffset);
      return false;
    }
    const auto *window = currentEditWindow();
    // Carve-out for the canonical "truncated/typoed word keyword" Replace.
    // We allow such a Replace to bypass the destructive-outside-window
    // guard because cascading recoveries (e.g. `statemachin` +
    // `initialStat` both typoed) otherwise hit the wall: the first commit
    // moves the active window past the second typo, so the second Replace
    // falls back to a synthetic Insert at a higher rule level which
    // leaves the typoed token unconsumed.
    //
    // Thresholds: cost ≤ 1 (single-codepoint edit), and the target literal
    // must be a word (≥ 3 codepoints). The literal gate is what keeps the
    // symbolic-literal guards in place — `>` → `=>` (literal length 2)
    // stays gated, `re` → `req` (length 3) and `initialStat` →
    // `initialState` (length 12) go through.
    constexpr std::uint32_t kSingleEditReplaceCostLimit = 1u;
    constexpr std::size_t kWordLiteralLengthFloor = 3u;
    const TextOffset replaceSpan =
        endOffset > cursorOffset() ? endOffset - cursorOffset() : 0;
    const auto *literalElement =
        element != nullptr &&
                element->getKind() == grammar::ElementKind::Literal
            ? static_cast<const grammar::Literal *>(element)
            : nullptr;
    const bool targetsWordKeyword =
        literalElement != nullptr &&
        literalElement->getValue().size() >= kWordLiteralLengthFloor;
    const bool singleEditFuzzyKeywordReplace =
        targetsWordKeyword &&
        replacementCost <= kSingleEditReplaceCostLimit && replaceSpan > 0;
    const bool destructiveEditOutsideActiveWindow =
        recoveryState.windowReplay.inRecoveryPhase && window != nullptr &&
        cursorOffset() > window->maxCursorOffset &&
        recoveryState.editBudget.hadEdits &&
        !allowDestructiveWindowContinuation &&
        !continues_local_edit_cluster_at_cursor() &&
        !singleEditFuzzyKeywordReplace;
    if (!canEdit() || destructiveEditOutsideActiveWindow ||
        !canEditAtOffset(endOffset) ||
        !canAffordEdit(replacementCost)) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    (void)beforeOffset;
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Replaced,
                             .offset = cursorOffset(),
                             .beginOffset = cursorOffset(),
                             .endOffset = endOffset,
                             .element = element});
    detail::apply_non_delete_edit_state(
        replacementCost, recoveryState.editBudget.editCost,
        recoveryState.editBudget.editCount,
        recoveryState.editBudget.hadEdits,
        recoveryState.editBudget.consecutiveDeletes);
    if (replayingCommittedReplace) {
      consumeCommittedRecoveryEdit();
    }
    noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind::Replaced);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextReplace);
    leaf(endPtr, element, hidden, true);
    PEGIUM_RECOVERY_TRACE("[rule] replace offset=", beforeOffset, " -> ",
                          cursorOffset(),
                          " kind=", static_cast<int>(element->getKind()));
    return true;
  }

private:
  friend struct TrackedParseContext;

  inline void afterTrackedSkip() noexcept {
    clearPendingDeleteHiddenTriviaBridge();
    refreshRecoveryPhase();
  }

  inline void afterTrackedLeaf(TextOffset beginOffset, TextOffset endOffset,
                               bool hidden) noexcept {
    clearPendingDeleteHiddenTriviaBridge();
    if (recoveryState.windowReplay.recoveryBookkeepingEnabled && !hidden &&
        endOffset > beginOffset) {
      onVisibleLeaf(beginOffset, endOffset);
    }
    refreshRecoveryPhase();
    if (trackEditState && recoveryState.editBudget.consecutiveDeletes != 0) {
      recoveryState.editBudget.consecutiveDeletes = 0;
    }
  }

  [[nodiscard]] constexpr const EditWindow *currentEditWindow() const noexcept {
    return hasPendingRecoveryWindows() ? &*editWindow : nullptr;
  }

  [[nodiscard]] constexpr EditWindow *currentEditWindowMutable() noexcept {
    return hasPendingRecoveryWindows() ? &*editWindow : nullptr;
  }

  [[nodiscard]] constexpr const RecoveryEdit *nextCommittedRecoveryEdit() const
      noexcept {
    return hasPendingCommittedRecoveryEdits()
               ? std::addressof(committedRecoveryEdits[committedRecoveryEditIndex])
               : nullptr;
  }

  [[nodiscard]] constexpr bool
  canReplayCommittedEditAtOffset(TextOffset offset) const noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    if (entry == nullptr) {
      return false;
    }
    switch (entry->kind) {
    case ParseDiagnosticKind::Inserted:
      return entry->beginOffset == offset && entry->endOffset == offset;
    case ParseDiagnosticKind::Deleted:
      // A committed Delete at [b, e) may replay from any cursor position
      // c < e: cursors c < b consume the gap between the prior edit and the
      // original delete span, producing a single merged Delete@[c, e) that
      // matches the behavior of the original accepted attempt.
      // (Permitting c < b is intentional and load-bearing for the
      // delete-merge replay path; the strict `matching_committed_delete`
      // predicate guards the actual replay site.)
      return offset < entry->endOffset;
    case ParseDiagnosticKind::Replaced:
      return offset >= entry->beginOffset && offset <= entry->endOffset;
    case ParseDiagnosticKind::Incomplete:
    case ParseDiagnosticKind::Recovered:
    case ParseDiagnosticKind::ConversionError:
      return false;
    }
    return false;
  }

  [[nodiscard]] constexpr bool
  matches_committed_insert(TextOffset offset,
                           const grammar::AbstractElement *element,
                           std::string_view message) const noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    return entry != nullptr && entry->kind == ParseDiagnosticKind::Inserted &&
           entry->beginOffset == offset && entry->endOffset == offset &&
           elements_equivalent_for_replay(entry->element, element) &&
           entry->message == message;
  }

  [[nodiscard]] constexpr const RecoveryEdit *
  matching_committed_delete(TextOffset offset) const noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    return entry != nullptr && entry->kind == ParseDiagnosticKind::Deleted &&
                   offset >= entry->beginOffset && offset < entry->endOffset
               ? entry
               : nullptr;
  }

  [[nodiscard]] constexpr bool
  matches_committed_replace(TextOffset beginOffset, TextOffset endOffset,
                            const grammar::AbstractElement *element) const
      noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    return entry != nullptr && entry->kind == ParseDiagnosticKind::Replaced &&
           entry->beginOffset == beginOffset && entry->endOffset == endOffset &&
           elements_equivalent_for_replay(entry->element, element);
  }

  inline void consumeCommittedRecoveryEdit() noexcept {
    if (!hasPendingCommittedRecoveryEdits()) {
      return;
    }
    ++committedRecoveryEditIndex;
  }

  inline void advanceCommittedDeleteReplay() noexcept {
    const auto *entry = nextCommittedRecoveryEdit();
    if (entry == nullptr || entry->kind != ParseDiagnosticKind::Deleted) {
      return;
    }
    if (cursorOffset() >= entry->endOffset) {
      consumeCommittedRecoveryEdit();
    }
  }

  [[nodiscard]] inline bool
  continues_local_edit_cluster_at_cursor() const noexcept {
    if (recoveryEdits.empty()) {
      return false;
    }
    const auto cursorOffsetValue = cursorOffset();
    const auto &lastEdit = recoveryEdits.back();
    return lastEdit.beginOffset == cursorOffsetValue &&
           lastEdit.endOffset == cursorOffsetValue;
  }

  [[nodiscard]] static constexpr std::uint32_t
  active_window_forward_token_budget(const EditWindow &window) noexcept {
    return std::max(window.forwardTokenCount, window.replayForwardTokenCount);
  }

  inline void
  noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind editKind) noexcept {
    if (!recoveryState.windowReplay.inRecoveryPhase) {
      return;
    }
    auto *window = currentEditWindowMutable();
    if (window == nullptr) {
      return;
    }
    const auto replayExtensionBase =
        editKind == ParseDiagnosticKind::Inserted
            ? stabilityTokenCount
            : window->forwardTokenCount;
    const auto extendedForwardTokenCount =
        static_cast<std::uint64_t>(
            recoveryState.windowReplay.currentForwardVisibleLeafCount) +
        static_cast<std::uint64_t>(replayExtensionBase);
    const auto nextReplayForwardTokenCount =
        static_cast<std::uint32_t>(
            std::min<std::uint64_t>(extendedForwardTokenCount,
                                    std::numeric_limits<std::uint32_t>::max()));
    if (nextReplayForwardTokenCount <= window->replayForwardTokenCount) {
      return;
    }
    window->replayForwardTokenCount = nextReplayForwardTokenCount;
  }

  [[nodiscard]] inline RecoveryEdit *pendingHiddenTriviaDeleteEdit() noexcept {
    if (_deleteBridge.pendingHiddenTriviaStart == nullptr ||
        _deleteBridge.pendingHiddenTriviaEnd == nullptr) {
      return nullptr;
    }
    if (_deleteBridge.pendingHiddenTriviaEnd != cursor() || recoveryEdits.empty()) {
      clearPendingDeleteHiddenTriviaBridge();
      return nullptr;
    }
    auto &lastEdit = recoveryEdits.back();
    if (lastEdit.kind != ParseDiagnosticKind::Deleted ||
        lastEdit.endOffset !=
            static_cast<TextOffset>(
                _deleteBridge.pendingHiddenTriviaStart - begin)) {
      clearPendingDeleteHiddenTriviaBridge();
      return nullptr;
    }
    return &lastEdit;
  }

  inline void clearPendingDeleteHiddenTriviaBridge() noexcept {
    if (_deleteBridge.pendingHiddenTriviaStart == nullptr &&
        _deleteBridge.pendingHiddenTriviaEnd == nullptr) {
      return;
    }
    _deleteBridge = {};
  }

  inline void beginActiveRecoveryWindow() noexcept {
    recoveryState.windowReplay.recoveryBookkeepingEnabled = true;
    _recordFailureHistory = true;
    recoveryState.windowReplay.inRecoveryPhase = true;
    recoveryState.windowReplay.activeWindowEditCostBase =
        recoveryState.editBudget.editCost;
    recoveryState.windowReplay.activeWindowEditCountBase =
        recoveryState.editBudget.editCount;
    recoveryState.windowReplay.currentForwardVisibleLeafCount = 0;
    recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.windowReplay.reachedRecoveryTarget = false;
    recoveryState.windowReplay.stableAfterRecovery = false;
    recoveryState.windowReplay.awaitingStrictStability = false;
    editFloorOffset = editWindow->editFloorOffset;
  }

  inline void completeActiveRecoveryWindow(bool reachedEof,
                                           bool handedOffToNextWindow = false) noexcept {
    ++recoveryState.windowReplay.completedRecoveryWindows;
    recoveryState.windowReplay.reachedRecoveryTarget =
        handedOffToNextWindow || reachedEof ||
        recoveryState.windowReplay.currentForwardVisibleLeafCount >=
            active_window_forward_token_budget(*editWindow);
    recoveryState.windowReplay.stableAfterRecovery = reachedEof;
    recoveryState.windowReplay.awaitingStrictStability =
        recoveryState.windowReplay.reachedRecoveryTarget &&
        !handedOffToNextWindow &&
        !recoveryState.windowReplay.stableAfterRecovery;
    recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.windowReplay.currentForwardVisibleLeafCount = 0;
    recoveryState.windowReplay.inRecoveryPhase = false;
    recoveryState.windowReplay.activeEditWindowCompleted = true;
    maybeDisableRecoveryBookkeeping();
  }

  inline void onVisibleLeaf(TextOffset beginOffset, TextOffset endOffset) noexcept {
    if (endOffset <= beginOffset) {
      return;
    }
    if (recoveryState.windowReplay.inRecoveryPhase) {
      if (!recoveryState.editBudget.hadEdits) {
        return;
      }
      const auto *window = currentEditWindow();
      if (window != nullptr && beginOffset >= window->maxCursorOffset) {
        ++recoveryState.windowReplay.currentForwardVisibleLeafCount;
      }
      return;
    }
    if (recoveryState.windowReplay.awaitingStrictStability) {
      ++recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery;
      if (recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery >=
          stabilityTokenCount) {
        recoveryState.windowReplay.stableAfterRecovery = true;
        recoveryState.windowReplay.awaitingStrictStability = false;
        maybeDisableRecoveryBookkeeping();
        return;
      }
    }
  }

  inline void maybeDisableRecoveryBookkeeping() noexcept {
    if (recoveryState.windowReplay.inRecoveryPhase ||
        recoveryState.windowReplay.awaitingStrictStability ||
        hasPendingRecoveryWindows()) {
      return;
    }
    recoveryState.windowReplay.recoveryBookkeepingEnabled = false;
  }

  detail::ActiveRecoveryStack _activeRecoveries;
};

inline void TrackedParseContext::skip() noexcept {
  const char *const before = _cursor;
  ParseContext::skip();
  if (_cursor > _maxCursor) [[likely]] {
    _maxCursor = _cursor;
  }
  if (_recordFailureHistory) [[likely]] {
    _failureRecorder.onCursor(cursor());
  }
  if (_runRecoveryBookkeeping) [[likely]] {
    // The two skipper variants converge to the same end for a given begin;
    // populate the no-builder cache so subsequent recovery probes at this
    // position skip the underlying terminal scan. Gated on
    // `_runRecoveryBookkeeping` because `skip_without_builder()` is called
    // exclusively from recovery code paths — the failure-only TrackedParseContext
    // never reads this cache.
    _skipCacheBegin = before;
    _skipCacheEnd = _cursor;
    static_cast<RecoveryContext &>(*this).afterTrackedSkip();
  }
}

inline void TrackedParseContext::leaf(
    const char *endPtr, const grammar::AbstractElement *element, bool hidden,
    bool recovered) {
  const auto beginOffset = cursorOffset();
  const auto endOffset = static_cast<TextOffset>(endPtr - begin);
  if (_recordFailureHistory) [[likely]] {
    _failureRecorder.onLeaf(cursor(), endPtr, element, hidden);
  }
  ParseContext::leaf(endPtr, element, hidden, recovered);
  if (_cursor > _maxCursor) [[likely]] {
    _maxCursor = _cursor;
  }
  if (_runRecoveryBookkeeping) [[likely]] {
    static_cast<RecoveryContext &>(*this).afterTrackedLeaf(beginOffset,
                                                           endOffset, hidden);
  }
}

inline void TrackedParseContext::leaf(
    const char *beginPtr, const char *endPtr,
    const grammar::AbstractElement *element, bool hidden, bool recovered) {
  const auto beginOffset = static_cast<TextOffset>(beginPtr - begin);
  const auto endOffset = static_cast<TextOffset>(endPtr - begin);
  if (_recordFailureHistory) [[likely]] {
    _failureRecorder.onLeaf(beginPtr, endPtr, element, hidden);
  }
  ParseContext::leaf(beginPtr, endPtr, element, hidden, recovered);
  if (_cursor > _maxCursor) [[likely]] {
    _maxCursor = _cursor;
  }
  if (_runRecoveryBookkeeping) [[likely]] {
    static_cast<RecoveryContext &>(*this).afterTrackedLeaf(beginOffset,
                                                           endOffset, hidden);
  }
}

} // namespace pegium::parser
