#pragma once

/// Strict, tracked, and recovery parse contexts used while building the CST.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <type_traits>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <pegium/core/parser/TextUtils.hpp>
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
        _lastVisibleCursor(begin), _maxCursor(begin), _builder(builder),
        _skipper(&skipper), _cancelToken(cancelToken) {
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
  [[nodiscard]] const char *skip_without_builder(const char *begin) const noexcept {
    return _skipper->skip(begin);
  }
  [[nodiscard]] constexpr const char *maxCursor() const noexcept {
    return _maxCursor;
  }

  constexpr void restoreMaxCursor(const char *cursor) noexcept {
    _maxCursor = cursor;
  }

  [[nodiscard]] constexpr TextOffset cursorOffset() const noexcept {
    return static_cast<TextOffset>(_cursor - begin);
  }

  [[nodiscard]] constexpr TextOffset lastVisibleCursorOffset() const noexcept {
    return static_cast<TextOffset>(_lastVisibleCursor - begin);
  }

  [[nodiscard]] constexpr TextOffset maxCursorOffset() const noexcept {
    return static_cast<TextOffset>(_maxCursor - begin);
  }

  [[nodiscard]] constexpr const utils::CancellationToken &
  cancellationToken() const noexcept {
    return _cancelToken;
  }
  inline void skip() noexcept {
    _cursor = _skipper->skip(_cursor, _builder);
    if (_cursor > _maxCursor) [[likely]] {
      _maxCursor = _cursor;
    }
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
    if (_cursor > _maxCursor) [[likely]] {
      _maxCursor = _cursor;
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
    if (_cursor > _maxCursor) [[likely]] {
      _maxCursor = _cursor;
    }
  }
  protected:
  const char *_cursor;
  const char *_lastVisibleCursor;
  const char *_maxCursor;
  CstBuilder &_builder;
  const Skipper *_skipper;
  const utils::CancellationToken &_cancelToken;


};

struct RecoveryContext;

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
      : ParseContext(builder, skipper, cancelToken),
        _failureRecorder(failureRecorder) {}

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
    if (_recordFailureHistory) [[unlikely]] {
      _failureRecorder.rewind(checkpoint.failureHistorySize);
    }
  }

  inline void skip() noexcept;

  inline void leaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false);
  inline void leaf(const char *beginPtr, const char *endPtr,
                   const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false);

  [[nodiscard]] constexpr bool isFailureHistoryRecordingEnabled() const noexcept {
    return _recordFailureHistory;
  }

  [[nodiscard]] std::size_t failureHistorySize() const noexcept {
    return _failureRecorder.mark();
  }

  [[nodiscard]] std::size_t furthestFailureHistorySize() const noexcept {
    return _failureRecorder.furthestVisibleLeafCount();
  }

protected:
  detail::FailureHistoryRecorder &_failureRecorder;
  bool _recordFailureHistory = true;
  bool _runRecoveryBookkeeping = false;
};

struct RecoveryContext : TrackedParseContext {
  static constexpr std::size_t kDefaultMaxConsecutiveCodepointDeletes = 8;
  static constexpr std::uint32_t kInternalRecoveryStabilityTokenCount = 2;

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
    std::uint32_t activeEditWindowIndex = 0;
    std::uint32_t currentForwardVisibleLeafCount = 0;
    std::uint32_t strictVisibleLeafCountAfterRecovery = 0;
    std::uint32_t completedRecoveryWindows = 0;
    bool inRecoveryPhase = true;
    bool reachedRecoveryTarget = false;
    bool stableAfterRecovery = false;
    bool awaitingStrictStability = false;
    bool recoveryBookkeepingEnabled = true;
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
    std::uint32_t recoveryStateVersion;
    std::uint32_t recoveryEditCount;
    std::uint32_t replayForwardTokenHistorySize;
    const char *localReplayMaxCursor;
    DeleteBridgeState deleteBridge;
  };

  class LeadingTerminalInsertScopeGuard {
  public:
    LeadingTerminalInsertScopeGuard(const LeadingTerminalInsertScopeGuard &) =
        delete;
    LeadingTerminalInsertScopeGuard &
    operator=(const LeadingTerminalInsertScopeGuard &) = delete;

    LeadingTerminalInsertScopeGuard(
        LeadingTerminalInsertScopeGuard &&other) noexcept
        : _ctx(other._ctx), _active(other._active) {
      other._active = false;
    }

    ~LeadingTerminalInsertScopeGuard() noexcept {
      if (_active) {
        --_ctx._scopedLeadingTerminalInsertDepth;
      }
    }

  private:
    friend struct RecoveryContext;

    explicit LeadingTerminalInsertScopeGuard(RecoveryContext &ctx) noexcept
        : _ctx(ctx) {
      ++_ctx._scopedLeadingTerminalInsertDepth;
    }

    RecoveryContext &_ctx;
    bool _active = true;
  };

  class DestructiveWindowContinuationGuard {
  public:
    DestructiveWindowContinuationGuard(
        const DestructiveWindowContinuationGuard &) = delete;
    DestructiveWindowContinuationGuard &
    operator=(const DestructiveWindowContinuationGuard &) = delete;

    DestructiveWindowContinuationGuard(
        DestructiveWindowContinuationGuard &&other) noexcept
        : _ctx(other._ctx), _active(other._active) {
      other._active = false;
    }

    ~DestructiveWindowContinuationGuard() noexcept {
      if (_active) {
        --_ctx._destructiveWindowContinuationDepth;
      }
    }

  private:
    friend struct RecoveryContext;

    explicit DestructiveWindowContinuationGuard(
        RecoveryContext &ctx) noexcept
        : _ctx(ctx) {
      ++_ctx._destructiveWindowContinuationDepth;
    }

    RecoveryContext &_ctx;
    bool _active = true;
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
    _localReplayMaxCursor = begin;
  }

  bool allowInsert = true;
  bool allowDelete = true;
  bool allowExtendedDeleteScan = true;
  bool allowDeleteRetry = true;
  bool stopInfixTailAfterRecovery = false;
  bool skipAfterDelete = true;
  bool trackEditState = true;
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
  TextOffset editFloorOffset = 0;
  std::vector<EditWindow> editWindows;
  bool allowTopLevelPartialSuccess = false;
  std::vector<RecoveryEdit> recoveryEdits;
  RecoveryState recoveryState;
  std::uint32_t recoveryStateVersion = 0;
  std::uint32_t _scopedLeadingTerminalInsertDepth = 0;
  std::uint32_t _destructiveWindowContinuationDepth = 0;
  const char *_localReplayMaxCursor = nullptr;
  DeleteBridgeState _deleteBridge{};
  struct ReplayForwardTokenDelta {
    std::uint32_t windowIndex = 0;
    std::uint32_t previousReplayForwardTokenCount = 1;
  };
  std::vector<ReplayForwardTokenDelta> _replayForwardTokenHistory;

  [[nodiscard]] constexpr const char *furthestExploredCursor() const noexcept {
    return maxCursor();
  }

  [[nodiscard]] constexpr TextOffset furthestExploredOffset() const noexcept {
    return maxCursorOffset();
  }

  [[nodiscard]] constexpr const char *localReplayMaxCursor() const noexcept {
    return _localReplayMaxCursor;
  }

  [[nodiscard]] constexpr TextOffset localReplayMaxOffset() const noexcept {
    return static_cast<TextOffset>(_localReplayMaxCursor - begin);
  }

  constexpr void restoreFurthestExploredCursor(const char *cursor) noexcept {
    restoreMaxCursor(cursor);
    if (_localReplayMaxCursor > maxCursor()) {
      _localReplayMaxCursor = maxCursor();
    }
  }

  constexpr void
  restoreLocalReplayMaxCursor(const char *cursorValue) noexcept {
    _localReplayMaxCursor =
        std::max(cursor(),
                 std::min(cursorValue, furthestExploredCursor()));
  }

  inline void refreshRecoveryPhase() noexcept {
    auto &windowReplay = recoveryState.windowReplay;
    if (!windowReplay.recoveryBookkeepingEnabled) {
      return;
    }
    if (windowReplay.awaitingStrictStability && cursor() == end) {
      windowReplay.stableAfterRecovery = true;
      windowReplay.awaitingStrictStability = false;
      ++recoveryStateVersion;
      maybeDisableRecoveryBookkeeping();
    }

    const auto windowCount = editWindows.size();
    if (windowCount == 0u) {
      return;
    }
    const auto *const windows = editWindows.data();

    while (true) {
      if (windowReplay.inRecoveryPhase) {
        const auto activeWindowIndex = windowReplay.activeEditWindowIndex;
        if (activeWindowIndex >= windowCount) {
          windowReplay.inRecoveryPhase = false;
          ++recoveryStateVersion;
          continue;
        }
        const auto &window = windows[activeWindowIndex];
        const auto nextWindowIndex = activeWindowIndex + 1u;
        if (nextWindowIndex < windowCount &&
            cursorOffset() >= windows[nextWindowIndex].editFloorOffset) {
          completeActiveRecoveryWindow(false, true);
          continue;
        }
        if (windowReplay.currentForwardVisibleLeafCount >=
            active_window_forward_token_budget(window)) {
          completeActiveRecoveryWindow(false);
          continue;
        }
        return;
      }

      const auto activeWindowIndex = windowReplay.activeEditWindowIndex;
      if (activeWindowIndex >= windowCount) {
        return;
      }
      if (cursorOffset() < windows[activeWindowIndex].beginOffset) {
        return;
      }
      beginActiveRecoveryWindow();
      return;
    }
  }

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    return {.parseCheckpoint = TrackedParseContext::mark(),
            .recoveryState = recoveryState,
            .recoveryStateVersion = recoveryStateVersion,
            .recoveryEditCount =
                static_cast<std::uint32_t>(recoveryEdits.size()),
            .replayForwardTokenHistorySize =
                static_cast<std::uint32_t>(_replayForwardTokenHistory.size()),
            .localReplayMaxCursor = _localReplayMaxCursor,
            .deleteBridge = _deleteBridge};
  }

  using TrackedParseContext::rewind;

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    TrackedParseContext::rewind(checkpoint.parseCheckpoint);
    _localReplayMaxCursor = checkpoint.localReplayMaxCursor;
    _deleteBridge = checkpoint.deleteBridge;
    restoreReplayForwardTokenCounts(checkpoint.replayForwardTokenHistorySize);
    if (recoveryStateVersion == checkpoint.recoveryStateVersion) {
      return;
    }
    recoveryState = checkpoint.recoveryState;
    recoveryEdits.resize(checkpoint.recoveryEditCount);
    recoveryStateVersion = checkpoint.recoveryStateVersion;
  }

  inline void exit(const char *checkpoint,
                   const grammar::AbstractElement *element) {
    TrackedParseContext::exit(checkpoint, element);
    if (trackEditState && recoveryState.editBudget.consecutiveDeletes != 0)
        [[unlikely]] {
      recoveryState.editBudget.consecutiveDeletes = 0;
      ++recoveryStateVersion;
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
    ++recoveryStateVersion;
  }

  void blockFrontier() noexcept {
    if (recoveryState.frontierBlocked) {
      return;
    }
    recoveryState.frontierBlocked = true;
    ++recoveryStateVersion;
  }

  void setEditWindows(std::vector<EditWindow> windows) noexcept {
    editWindows = std::move(windows);
    _replayForwardTokenHistory.clear();
    recoveryState.windowReplay.activeWindowEditCostBase =
        recoveryState.editBudget.editCost;
    recoveryState.windowReplay.activeWindowEditCountBase =
        recoveryState.editBudget.editCount;
    recoveryState.windowReplay.activeEditWindowIndex = 0;
    recoveryState.windowReplay.currentForwardVisibleLeafCount = 0;
    recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.windowReplay.completedRecoveryWindows = 0;
    recoveryState.windowReplay.reachedRecoveryTarget = false;
    recoveryState.windowReplay.stableAfterRecovery = false;
    recoveryState.windowReplay.awaitingStrictStability = false;
    recoveryState.windowReplay.inRecoveryPhase = false;
    recoveryState.windowReplay.recoveryBookkeepingEnabled = !editWindows.empty();
    _recordFailureHistory = recoveryState.windowReplay.recoveryBookkeepingEnabled;
    if (!editWindows.empty()) {
      editFloorOffset = editWindows.front().editFloorOffset;
    } else {
      editFloorOffset = 0;
    }
    _localReplayMaxCursor = cursor();
    _deleteBridge = {};
    ++recoveryStateVersion;
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
      ++recoveryStateVersion;
      maybeDisableRecoveryBookkeeping();
    }
  }
  [[nodiscard]] constexpr bool isInRecoveryPhase() const noexcept {
    return recoveryState.windowReplay.inRecoveryPhase;
  }
  [[nodiscard]] constexpr bool hasPendingRecoveryWindows() const noexcept {
    return recoveryState.windowReplay.activeEditWindowIndex < editWindows.size();
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowBeginOffset() const noexcept {
    return recoveryState.windowReplay.activeEditWindowIndex < editWindows.size()
               ? editWindows[recoveryState.windowReplay.activeEditWindowIndex]
                     .beginOffset
               : 0;
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowMaxCursorOffset() const noexcept {
    return recoveryState.windowReplay.activeEditWindowIndex < editWindows.size()
               ? editWindows[recoveryState.windowReplay.activeEditWindowIndex]
                     .maxCursorOffset
               : 0;
  }
  [[nodiscard]] std::size_t activeRecoveryDepth() const noexcept {
    return _activeRecoveries.size();
  }
  [[nodiscard]] constexpr bool hasHadEdits() const noexcept {
    return recoveryState.editBudget.hadEdits;
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
    return detail::is_active_recovery(_activeRecoveries, *this, element);
  }

  [[nodiscard]] ActiveRecoveryGuard
  enterActiveRecovery(const grammar::AbstractElement *element) noexcept {
    return detail::enter_active_recovery(_activeRecoveries, *this, element);
  }

  [[nodiscard]] EditStateGuard
  withEditPermissions(bool nextAllowInsert, bool nextAllowDelete) noexcept {
    return detail::make_edit_permissions_guard(*this, nextAllowInsert,
                                               nextAllowDelete);
  }

  [[nodiscard]] EditStateGuard
  withEditState(bool nextAllowInsert, bool nextAllowDelete,
                bool nextTrackEditState) noexcept {
    return detail::make_edit_state_guard(*this, nextAllowInsert,
                                         nextAllowDelete,
                                         nextTrackEditState);
  }

  [[nodiscard]] LeadingTerminalInsertScopeGuard
  withLeadingTerminalInsertScope() noexcept {
    return LeadingTerminalInsertScopeGuard{*this};
  }

  [[nodiscard]] DestructiveWindowContinuationGuard
  withDestructiveWindowContinuation() noexcept {
    return DestructiveWindowContinuationGuard{*this};
  }

  [[nodiscard]] constexpr bool
  allowsScopedLeadingTerminalInsertRecovery() const noexcept {
    return _scopedLeadingTerminalInsertDepth != 0u;
  }

  [[nodiscard]] constexpr bool canEditAtOffset(TextOffset offset) const noexcept {
    if (editWindows.empty()) {
      return recoveryState.windowReplay.inRecoveryPhase &&
             offset >= editFloorOffset;
    }
    if (recoveryState.windowReplay.inRecoveryPhase &&
        recoveryState.windowReplay.activeEditWindowIndex < editWindows.size()) {
      const auto &window =
          editWindows[recoveryState.windowReplay.activeEditWindowIndex];
      return offset >= window.editFloorOffset;
    }
    return false;
  }

  [[nodiscard]] constexpr bool canEdit() const noexcept {
    return canEditAtOffset(cursorOffset());
  }


  [[nodiscard]] constexpr bool canInsert() const noexcept {
    return detail::can_insert(allowInsert, canEdit());
  }

  [[nodiscard]] constexpr bool canDelete() const noexcept {
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
    replayCounts.reserve(editWindows.size());
    for (const auto &window : editWindows) {
      replayCounts.push_back(
          std::max(window.forwardTokenCount, window.replayForwardTokenCount));
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
      ++recoveryStateVersion;
    }
  }

  bool insertSynthetic(const grammar::AbstractElement *element) {
    if (!trackEditState) {
      return false;
    }
    clearPendingDeleteHiddenTriviaBridge();
    if (!canInsert() || !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      PEGIUM_RECOVERY_TRACE("[rule] insert blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Inserted,
                             .offset = cursorOffset(),
                             .beginOffset = cursorOffset(),
                             .endOffset = cursorOffset(),
                             .element = element});
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted),
        recoveryState.editBudget.editCost, recoveryState.editBudget.editCount,
        recoveryState.editBudget.hadEdits,
        recoveryState.editBudget.consecutiveDeletes);
    noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind::Inserted);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextInsert);
    PEGIUM_RECOVERY_TRACE("[rule] insert synthetic offset=", cursorOffset(),
                          " kind=", static_cast<int>(element->getKind()));
    ++recoveryStateVersion;
    return true;
  }

  bool insertSyntheticGapAt(const char *position,
                            const char *message = nullptr) {
    if (!trackEditState || position < begin || position > end) {
      return false;
    }
    clearPendingDeleteHiddenTriviaBridge();
    const auto offset = static_cast<TextOffset>(position - begin);
    if (!detail::can_insert(allowInsert, canEditAtOffset(offset)) ||
        !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      PEGIUM_RECOVERY_TRACE("[rule] insert synthetic gap blocked offset=", offset,
                            " floor=", editFloorOffset);
      return false;
    }
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Inserted,
                             .offset = offset,
                             .beginOffset = offset,
                             .endOffset = offset,
                             .element = nullptr,
                             .message =
                                 message == nullptr ? std::string{}
                                                    : std::string(message)});
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted),
        recoveryState.editBudget.editCost, recoveryState.editBudget.editCount,
        recoveryState.editBudget.hadEdits,
        recoveryState.editBudget.consecutiveDeletes);
    noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind::Inserted);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextInsert);
    PEGIUM_RECOVERY_TRACE("[rule] insert synthetic gap offset=", offset);
    ++recoveryStateVersion;
    return true;
  }

  bool deleteOneCodepoint() noexcept {
    if (!trackEditState) {
      return false;
    }
    auto *mergedDeleteEdit = pendingHiddenTriviaDeleteEdit();
    const auto *window = currentEditWindow();
    const bool destructiveEditOutsideActiveWindow =
        recoveryState.windowReplay.inRecoveryPhase && window != nullptr &&
        cursorOffset() > window->maxCursorOffset &&
        recoveryState.editBudget.hadEdits &&
        _destructiveWindowContinuationDepth == 0u &&
        !continues_local_edit_cluster_at_cursor() &&
        recoveryState.editBudget.consecutiveDeletes == 0u;
    if (!canDelete() || destructiveEditOutsideActiveWindow ||
        !canAffordEdit(ParseDiagnosticKind::Deleted)) {
      clearPendingDeleteHiddenTriviaBridge();
      PEGIUM_RECOVERY_TRACE("[rule] delete blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " consecutive=",
                            recoveryState.editBudget.consecutiveDeletes,
                            "/",
                            maxConsecutiveCodepointDeletes);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    (void)beforeOffset;
    const char *const deletedEnd = detail::next_codepoint_cursor(cursor());
    const char *const next = deletedEnd;
    if (next <= cursor()) [[unlikely]] {
      clearPendingDeleteHiddenTriviaBridge();
      return false;
    }
    if (mergedDeleteEdit != nullptr) {
      mergedDeleteEdit->endOffset =
          static_cast<TextOffset>(deletedEnd - begin);
      _deleteBridge = {};
    } else {
      recoveryEdits.push_back({.kind = ParseDiagnosticKind::Deleted,
                               .offset = cursorOffset(),
                               .beginOffset = cursorOffset(),
                               .endOffset =
                                   static_cast<TextOffset>(deletedEnd - begin),
                               .element = nullptr});
    }
    detail::apply_delete_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Deleted),
        recoveryState.editBudget.editCost, recoveryState.editBudget.editCount,
        recoveryState.editBudget.hadEdits,
        recoveryState.editBudget.consecutiveDeletes);
    noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind::Deleted);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextDelete);

    _cursor = next;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    noteLocalReplayCursorAdvance();
    ++recoveryStateVersion;
    if (skipAfterDelete) {
      skip();
    }
    PEGIUM_RECOVERY_TRACE("[rule] delete offset=", beforeOffset, " -> ",
                          cursorOffset());
    return true;
  }

  bool extendLastDeleteThroughHiddenTrivia() noexcept {
    if (!trackEditState || recoveryEdits.empty() || cursor() >= end) {
      return false;
    }
    clearPendingDeleteHiddenTriviaBridge();
    auto &lastEdit = recoveryEdits.back();
    if (lastEdit.kind != ParseDiagnosticKind::Deleted ||
        lastEdit.endOffset != cursorOffset()) {
      return false;
    }

    const char *const hiddenEnd = skip_without_builder(cursor());
    if (hiddenEnd <= cursor()) {
      return false;
    }
    if (hiddenEnd >= end) {
      return false;
    }
    const char *const previousDeletedCursor =
        detail::previous_codepoint_cursor(begin, cursor());
    const bool previousDeletedIsWordLike =
        detail::is_identifier_like_codepoint(
            static_cast<unsigned char>(*previousDeletedCursor));
    const bool nextVisibleIsWordLike = detail::is_identifier_like_codepoint(
        static_cast<unsigned char>(*hiddenEnd));
    if (previousDeletedIsWordLike != nextVisibleIsWordLike) {
      return false;
    }

    _deleteBridge.pendingHiddenTriviaStart = cursor();
    _deleteBridge.pendingHiddenTriviaEnd = hiddenEnd;
    _cursor = hiddenEnd;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    noteLocalReplayCursorAdvance();
    ++recoveryStateVersion;
    PEGIUM_RECOVERY_TRACE("[rule] bridge delete through hidden trivia -> ",
                          cursorOffset());
    return true;
  }

  bool replaceLeaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false) {
    return replaceLeaf(endPtr, element,
                       detail::default_edit_cost(ParseDiagnosticKind::Replaced),
                       hidden);
  }

  bool replaceLeaf(const char *endPtr, const grammar::AbstractElement *element,
                   std::uint32_t replacementCost, bool hidden) {
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
    const auto *window = currentEditWindow();
    const bool destructiveEditOutsideActiveWindow =
        recoveryState.windowReplay.inRecoveryPhase && window != nullptr &&
        cursorOffset() > window->maxCursorOffset &&
        recoveryState.editBudget.hadEdits &&
        _destructiveWindowContinuationDepth == 0u &&
        !continues_local_edit_cluster_at_cursor();
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
    detail::apply_replace_edit_state(
        replacementCost, recoveryState.editBudget.editCost,
        recoveryState.editBudget.editCount,
        recoveryState.editBudget.hadEdits,
        recoveryState.editBudget.consecutiveDeletes);
    noteReplayForwardRequirementForCurrentWindow(ParseDiagnosticKind::Replaced);
    PEGIUM_STEP_TRACE_INC(detail::StepCounter::ParseContextReplace);
    leaf(endPtr, element, hidden, true);
    ++recoveryStateVersion;
    PEGIUM_RECOVERY_TRACE("[rule] replace offset=", beforeOffset, " -> ",
                          cursorOffset(),
                          " kind=", static_cast<int>(element->getKind()));
    return true;
  }

private:
  friend struct TrackedParseContext;

  inline void afterTrackedSkip() noexcept {
    clearPendingDeleteHiddenTriviaBridge();
    noteLocalReplayCursorAdvance();
    refreshRecoveryPhase();
  }

  inline void afterTrackedLeaf(TextOffset beginOffset, TextOffset endOffset,
                               bool hidden) noexcept {
    clearPendingDeleteHiddenTriviaBridge();
    noteLocalReplayCursorAdvance();
    if (recoveryState.windowReplay.recoveryBookkeepingEnabled && !hidden &&
        endOffset > beginOffset) {
      onVisibleLeaf(beginOffset, endOffset);
    }
    refreshRecoveryPhase();
    if (trackEditState && recoveryState.editBudget.consecutiveDeletes != 0) {
      recoveryState.editBudget.consecutiveDeletes = 0;
      ++recoveryStateVersion;
    }
  }

  [[nodiscard]] constexpr const EditWindow *currentEditWindow() const noexcept {
    const auto activeWindowIndex = recoveryState.windowReplay.activeEditWindowIndex;
    return activeWindowIndex < editWindows.size()
               ? editWindows.data() + activeWindowIndex
               : nullptr;
  }

  [[nodiscard]] constexpr EditWindow *currentEditWindowMutable() noexcept {
    const auto activeWindowIndex = recoveryState.windowReplay.activeEditWindowIndex;
    return activeWindowIndex < editWindows.size()
               ? editWindows.data() + activeWindowIndex
               : nullptr;
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
            ? kInternalRecoveryStabilityTokenCount
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
    const auto windowIndex =
        recoveryState.windowReplay.activeEditWindowIndex;
    if (windowIndex >= editWindows.size()) {
      return;
    }
    _replayForwardTokenHistory.push_back(
        {.windowIndex = windowIndex,
         .previousReplayForwardTokenCount = window->replayForwardTokenCount});
    window->replayForwardTokenCount = nextReplayForwardTokenCount;
  }

  inline void noteLocalReplayCursorAdvance() noexcept {
    if (_localReplayMaxCursor < cursor()) {
      _localReplayMaxCursor = cursor();
    }
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
    ++recoveryStateVersion;
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
    editFloorOffset =
        editWindows[recoveryState.windowReplay.activeEditWindowIndex]
            .editFloorOffset;
    ++recoveryStateVersion;
  }

  inline void completeActiveRecoveryWindow(bool reachedEof,
                                           bool handedOffToNextWindow = false) noexcept {
    ++recoveryState.windowReplay.completedRecoveryWindows;
    const auto &window =
        editWindows[recoveryState.windowReplay.activeEditWindowIndex];
    recoveryState.windowReplay.reachedRecoveryTarget =
        handedOffToNextWindow || reachedEof ||
        recoveryState.windowReplay.currentForwardVisibleLeafCount >=
            active_window_forward_token_budget(window);
    recoveryState.windowReplay.stableAfterRecovery = reachedEof;
    recoveryState.windowReplay.awaitingStrictStability =
        recoveryState.windowReplay.reachedRecoveryTarget &&
        !handedOffToNextWindow &&
        !recoveryState.windowReplay.stableAfterRecovery;
    recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.windowReplay.currentForwardVisibleLeafCount = 0;
    recoveryState.windowReplay.inRecoveryPhase = false;
    ++recoveryState.windowReplay.activeEditWindowIndex;
    if (recoveryState.windowReplay.activeEditWindowIndex < editWindows.size()) {
      editFloorOffset =
          editWindows[recoveryState.windowReplay.activeEditWindowIndex]
              .editFloorOffset;
    }
    ++recoveryStateVersion;
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
        ++recoveryStateVersion;
      }
      return;
    }
    if (recoveryState.windowReplay.awaitingStrictStability) {
      ++recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery;
      if (recoveryState.windowReplay.strictVisibleLeafCountAfterRecovery >=
          kInternalRecoveryStabilityTokenCount) {
        recoveryState.windowReplay.stableAfterRecovery = true;
        recoveryState.windowReplay.awaitingStrictStability = false;
        ++recoveryStateVersion;
        maybeDisableRecoveryBookkeeping();
        return;
      }
      ++recoveryStateVersion;
    }
  }

  inline void maybeDisableRecoveryBookkeeping() noexcept {
    if (recoveryState.windowReplay.inRecoveryPhase ||
        recoveryState.windowReplay.awaitingStrictStability ||
        recoveryState.windowReplay.activeEditWindowIndex < editWindows.size()) {
      return;
    }
    recoveryState.windowReplay.recoveryBookkeepingEnabled = false;
    ++recoveryStateVersion;
  }

  void restoreReplayForwardTokenCounts(
      std::uint32_t replayForwardTokenHistorySize) noexcept {
    if (_replayForwardTokenHistory.size() <= replayForwardTokenHistorySize) {
      return;
    }
    while (_replayForwardTokenHistory.size() > replayForwardTokenHistorySize) {
      const auto delta = _replayForwardTokenHistory.back();
      _replayForwardTokenHistory.pop_back();
      if (delta.windowIndex < editWindows.size()) {
        editWindows[delta.windowIndex].replayForwardTokenCount =
            delta.previousReplayForwardTokenCount;
      }
    }
  }

  detail::ActiveRecoveryStack _activeRecoveries;
};

inline void TrackedParseContext::skip() noexcept {
  ParseContext::skip();
  if (_recordFailureHistory) [[unlikely]] {
    _failureRecorder.onCursor(cursor());
  }
  if (_runRecoveryBookkeeping) [[unlikely]] {
    static_cast<RecoveryContext &>(*this).afterTrackedSkip();
  }
}

inline void TrackedParseContext::leaf(
    const char *endPtr, const grammar::AbstractElement *element, bool hidden,
    bool recovered) {
  const auto beginOffset = cursorOffset();
  const auto endOffset = static_cast<TextOffset>(endPtr - begin);
  if (_recordFailureHistory) [[unlikely]] {
    _failureRecorder.onLeaf(cursor(), endPtr, element, hidden);
  }
  ParseContext::leaf(endPtr, element, hidden, recovered);
  if (_runRecoveryBookkeeping) [[unlikely]] {
    static_cast<RecoveryContext &>(*this).afterTrackedLeaf(beginOffset,
                                                           endOffset, hidden);
  }
}

inline void TrackedParseContext::leaf(
    const char *beginPtr, const char *endPtr,
    const grammar::AbstractElement *element, bool hidden, bool recovered) {
  const auto beginOffset = static_cast<TextOffset>(beginPtr - begin);
  const auto endOffset = static_cast<TextOffset>(endPtr - begin);
  if (_recordFailureHistory) [[unlikely]] {
    _failureRecorder.onLeaf(beginPtr, endPtr, element, hidden);
  }
  ParseContext::leaf(beginPtr, endPtr, element, hidden, recovered);
  if (_runRecoveryBookkeeping) [[unlikely]] {
    static_cast<RecoveryContext &>(*this).afterTrackedLeaf(beginOffset,
                                                           endOffset, hidden);
  }
}

} // namespace pegium::parser
