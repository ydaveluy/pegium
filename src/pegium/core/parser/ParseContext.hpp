#pragma once

/// Strict, tracked, and recovery parse contexts used while building the CST.

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/ContextShared.hpp>
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
    detail::stepTraceInc(detail::StepCounter::ParseContextMark);

    return {.cursor = _cursor,
            .lastVisibleCursor = _lastVisibleCursor,
            .builder = _builder.mark()};
  }

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    detail::stepTraceInc(detail::StepCounter::ParseContextRewind);
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
    detail::stepTraceInc(detail::StepCounter::ParseContextEnter);
    _builder.enter();

    return _cursor;
  }

  inline void exit(const char *checkpoint,
                   const grammar::AbstractElement *element) {
    detail::stepTraceInc(detail::StepCounter::ParseContextExit);
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
    detail::stepTraceInc(detail::StepCounter::ParseContextLeaf);
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

  [[nodiscard]] constexpr bool isFailureHistoryRecordingEnabled() const noexcept {
    return _recordFailureHistory;
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
    TextOffset maxCursorOffset = 0;
    std::uint32_t forwardTokenCount = 0;
  };

  struct RecoveryEdit {
    ParseDiagnosticKind kind = ParseDiagnosticKind::Deleted;
    TextOffset offset = 0;
    TextOffset endOffset = 0;
    const grammar::AbstractElement *element = nullptr;
    const char *message = nullptr;
  };

  struct RecoveryState {
    std::uint32_t consecutiveDeletes = 0;
    std::uint32_t editCost = 0;
    std::uint32_t editCount = 0;
    std::uint32_t activeEditWindowIndex = 0;
    std::uint32_t currentForwardVisibleLeafCount = 0;
    std::uint32_t strictVisibleLeafCountAfterRecovery = 0;
    std::uint32_t completedRecoveryWindows = 0;
    bool inRecoveryPhase = true;
    bool hadEdits = false;
    bool reachedRecoveryTarget = false;
    bool stableAfterRecovery = false;
    bool awaitingStrictStability = false;
    bool recoveryBookkeepingEnabled = true;
  };

  static_assert(std::is_trivially_copyable_v<RecoveryState>);

  struct Checkpoint {
    TrackedParseContext::Checkpoint parseCheckpoint;
    RecoveryState recoveryState;
    std::uint32_t recoveryStateVersion;
    std::uint32_t recoveryEditCount;
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

  inline void refreshRecoveryPhase() noexcept {
    if (!recoveryState.recoveryBookkeepingEnabled) {
      return;
    }
    if (recoveryState.awaitingStrictStability && cursor() == end) {
      recoveryState.stableAfterRecovery = true;
      recoveryState.awaitingStrictStability = false;
      ++recoveryStateVersion;
      maybeDisableRecoveryBookkeeping();
    }

    if (editWindows.empty()) {
      return;
    }

    while (true) {
      if (recoveryState.inRecoveryPhase) {
        const auto *window = currentEditWindow();
        if (window == nullptr) {
          recoveryState.inRecoveryPhase = false;
          ++recoveryStateVersion;
          continue;
        }
        if (recoveryState.currentForwardVisibleLeafCount >=
            window->forwardTokenCount) {
          completeActiveRecoveryWindow(false);
          continue;
        }
        return;
      }

      if (recoveryState.activeEditWindowIndex >= editWindows.size()) {
        return;
      }
      if (cursorOffset() <
          editWindows[recoveryState.activeEditWindowIndex].beginOffset) {
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
                static_cast<std::uint32_t>(recoveryEdits.size())};
  }

  using TrackedParseContext::rewind;

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    TrackedParseContext::rewind(checkpoint.parseCheckpoint);
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
    if (trackEditState && recoveryState.consecutiveDeletes != 0) [[unlikely]] {
      recoveryState.consecutiveDeletes = 0;
      ++recoveryStateVersion;
    }
  }

  void setEditWindows(std::vector<EditWindow> windows) noexcept {
    editWindows = std::move(windows);
    recoveryState.activeEditWindowIndex = 0;
    recoveryState.currentForwardVisibleLeafCount = 0;
    recoveryState.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.completedRecoveryWindows = 0;
    recoveryState.reachedRecoveryTarget = false;
    recoveryState.stableAfterRecovery = false;
    recoveryState.awaitingStrictStability = false;
    recoveryState.inRecoveryPhase = false;
    recoveryState.recoveryBookkeepingEnabled = !editWindows.empty();
    _recordFailureHistory = recoveryState.recoveryBookkeepingEnabled;
    if (!editWindows.empty()) {
      editFloorOffset = editWindows.front().beginOffset;
    } else {
      editFloorOffset = 0;
    }
    ++recoveryStateVersion;
  }
  inline void finalizeRecoveryAtEof() noexcept {
    if (cursor() != end) {
      return;
    }
    if (recoveryState.inRecoveryPhase && currentEditWindow() != nullptr) {
      completeActiveRecoveryWindow(true);
      return;
    }
    if (recoveryState.awaitingStrictStability) {
      recoveryState.stableAfterRecovery = true;
      recoveryState.awaitingStrictStability = false;
      ++recoveryStateVersion;
      maybeDisableRecoveryBookkeeping();
    }
  }
  [[nodiscard]] constexpr bool isInRecoveryPhase() const noexcept {
    return recoveryState.inRecoveryPhase;
  }
  [[nodiscard]] constexpr bool hasPendingRecoveryWindows() const noexcept {
    return recoveryState.activeEditWindowIndex < editWindows.size();
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowBeginOffset() const noexcept {
    return recoveryState.activeEditWindowIndex < editWindows.size()
               ? editWindows[recoveryState.activeEditWindowIndex].beginOffset
               : 0;
  }
  [[nodiscard]] constexpr TextOffset
  pendingRecoveryWindowMaxCursorOffset() const noexcept {
    return recoveryState.activeEditWindowIndex < editWindows.size()
               ? editWindows[recoveryState.activeEditWindowIndex]
                     .maxCursorOffset
               : 0;
  }
  [[nodiscard]] std::size_t activeRecoveryDepth() const noexcept {
    return _activeRecoveries.size();
  }
  [[nodiscard]] constexpr bool hasHadEdits() const noexcept {
    return recoveryState.hadEdits;
  }
  [[nodiscard]] constexpr std::uint32_t
  completedRecoveryWindowCount() const noexcept {
    return recoveryState.completedRecoveryWindows;
  }
  [[nodiscard]] constexpr bool hasReachedRecoveryTarget() const noexcept {
    return recoveryState.reachedRecoveryTarget;
  }
  [[nodiscard]] constexpr bool isStableAfterRecovery() const noexcept {
    return recoveryState.stableAfterRecovery;
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

  [[nodiscard]] constexpr bool canEditAtOffset(TextOffset offset) const noexcept {
    if (editWindows.empty()) {
      return recoveryState.inRecoveryPhase && offset >= editFloorOffset;
    }
    if (!recoveryState.inRecoveryPhase ||
        recoveryState.activeEditWindowIndex >= editWindows.size()) {
      return false;
    }
    return offset >= editWindows[recoveryState.activeEditWindowIndex].beginOffset;
  }

  [[nodiscard]] constexpr bool canEdit() const noexcept {
    return canEditAtOffset(cursorOffset());
  }

  [[nodiscard]] constexpr bool canInsert() const noexcept {
    return detail::can_insert(allowInsert, canEdit());
  }

  [[nodiscard]] constexpr bool canDelete() const noexcept {
    return detail::can_delete(allowDelete, canEdit(),
                              recoveryState.consecutiveDeletes,
                              maxConsecutiveCodepointDeletes, *cursor());
  }

  [[nodiscard]] constexpr std::uint32_t currentEditCost() const noexcept {
    return recoveryState.editCost;
  }
  [[nodiscard]] constexpr std::uint32_t currentEditCount() const noexcept {
    return recoveryState.editCount;
  }

  [[nodiscard]] constexpr std::uint32_t
  editCostDelta(const Checkpoint &checkpoint) const noexcept {
    return recoveryState.editCost - checkpoint.recoveryState.editCost;
  }

  [[nodiscard]] std::size_t recoveryEditCount() const noexcept {
    return recoveryEdits.size();
  }

  [[nodiscard]] std::vector<RecoveryEdit> snapshotRecoveryEdits() const {
    return recoveryEdits;
  }

  [[nodiscard]] std::vector<RecoveryEdit> takeRecoveryEdits() noexcept {
    return std::move(recoveryEdits);
  }

  [[nodiscard]] static std::vector<ParseDiagnostic>
  materializeRecoveryEdits(const std::vector<RecoveryEdit> &edits) {
    std::vector<ParseDiagnostic> diagnostics;
    diagnostics.reserve(edits.size());
    for (const auto &edit : edits) {
      diagnostics.push_back({.kind = edit.kind,
                             .offset = edit.offset,
                             .beginOffset = edit.offset,
                             .endOffset = edit.endOffset,
                             .element = edit.element,
                             .message = edit.message == nullptr
                                            ? std::string{}
                                            : std::string(edit.message)});
    }
    return normalizeParseDiagnostics(diagnostics);
  }

  [[nodiscard]] constexpr bool
  canAffordEdit(ParseDiagnosticKind kind) const noexcept {
    return detail::can_afford_edit(recoveryState.editCount,
                                   recoveryState.editCost,
                                   maxEditsPerAttempt, maxEditCost, kind);
  }
  [[nodiscard]] constexpr bool
  canAffordEdit(std::uint32_t customCost) const noexcept {
    return detail::can_afford_edit(recoveryState.editCount,
                                   recoveryState.editCost,
                                   maxEditsPerAttempt, maxEditCost,
                                   customCost);
  }

  bool insertSynthetic(const grammar::AbstractElement *element) {
    if (!trackEditState) {
      return false;
    }
    if (!canInsert() || !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      PEGIUM_RECOVERY_TRACE("[rule] insert blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Inserted,
                             .offset = cursorOffset(),
                             .endOffset = cursorOffset(),
                             .element = element});
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted),
        recoveryState.editCost, recoveryState.editCount,
        recoveryState.hadEdits, recoveryState.consecutiveDeletes);
    detail::stepTraceInc(detail::StepCounter::ParseContextInsert);
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
    const auto offset = static_cast<TextOffset>(position - begin);
    if (!detail::can_insert(allowInsert, canEditAtOffset(offset)) ||
        !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      PEGIUM_RECOVERY_TRACE("[rule] insert synthetic gap blocked offset=", offset,
                            " floor=", editFloorOffset);
      return false;
    }
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Inserted,
                             .offset = offset,
                             .endOffset = offset,
                             .element = nullptr,
                             .message = message});
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted),
        recoveryState.editCost, recoveryState.editCount,
        recoveryState.hadEdits, recoveryState.consecutiveDeletes);
    detail::stepTraceInc(detail::StepCounter::ParseContextInsert);
    PEGIUM_RECOVERY_TRACE("[rule] insert synthetic gap offset=", offset);
    ++recoveryStateVersion;
    return true;
  }

  bool deleteOneCodepoint() noexcept {
    if (!trackEditState) {
      return false;
    }
    if (!canDelete() || !canAffordEdit(ParseDiagnosticKind::Deleted)) {
      PEGIUM_RECOVERY_TRACE("[rule] delete blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " consecutive=", recoveryState.consecutiveDeletes,
                            "/",
                            maxConsecutiveCodepointDeletes);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    (void)beforeOffset;
    const char *const deletedEnd = detail::next_codepoint_cursor(cursor());
    const char *const next = deletedEnd;
    if (next <= cursor()) [[unlikely]] {
      return false;
    }
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Deleted,
                             .offset = cursorOffset(),
                             .endOffset =
                                 static_cast<TextOffset>(deletedEnd - begin),
                             .element = nullptr});
    detail::apply_delete_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Deleted),
        recoveryState.editCost, recoveryState.editCount,
        recoveryState.hadEdits, recoveryState.consecutiveDeletes);
    detail::stepTraceInc(detail::StepCounter::ParseContextDelete);

    _cursor = next;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    ++recoveryStateVersion;
    skip();
    PEGIUM_RECOVERY_TRACE("[rule] delete offset=", beforeOffset, " -> ",
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
    if (endPtr <= cursor() || endPtr > end) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    const auto endOffset = static_cast<TextOffset>(endPtr - begin);
    if (!canEdit() || !canEditAtOffset(endOffset) ||
        !canAffordEdit(replacementCost)) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    (void)beforeOffset;
    recoveryEdits.push_back({.kind = ParseDiagnosticKind::Replaced,
                             .offset = cursorOffset(),
                             .endOffset = endOffset,
                             .element = element});
    detail::apply_replace_edit_state(
        replacementCost, recoveryState.editCost, recoveryState.editCount,
        recoveryState.hadEdits, recoveryState.consecutiveDeletes);
    detail::stepTraceInc(detail::StepCounter::ParseContextReplace);
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
    refreshRecoveryPhase();
  }

  inline void afterTrackedLeaf(TextOffset beginOffset, TextOffset endOffset,
                               bool hidden) noexcept {
    if (recoveryState.recoveryBookkeepingEnabled && !hidden &&
        endOffset > beginOffset) {
      onVisibleLeaf(beginOffset, endOffset);
    }
    refreshRecoveryPhase();
    if (trackEditState && recoveryState.consecutiveDeletes != 0) {
      recoveryState.consecutiveDeletes = 0;
      ++recoveryStateVersion;
    }
  }

  [[nodiscard]] constexpr const EditWindow *currentEditWindow() const noexcept {
    return recoveryState.activeEditWindowIndex < editWindows.size()
               ? &editWindows[recoveryState.activeEditWindowIndex]
               : nullptr;
  }

  inline void beginActiveRecoveryWindow() noexcept {
    recoveryState.recoveryBookkeepingEnabled = true;
    _recordFailureHistory = true;
    recoveryState.inRecoveryPhase = true;
    recoveryState.currentForwardVisibleLeafCount = 0;
    recoveryState.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.reachedRecoveryTarget = false;
    recoveryState.stableAfterRecovery = false;
    recoveryState.awaitingStrictStability = false;
    editFloorOffset = editWindows[recoveryState.activeEditWindowIndex].beginOffset;
    ++recoveryStateVersion;
  }

  inline void completeActiveRecoveryWindow(bool reachedEof) noexcept {
    ++recoveryState.completedRecoveryWindows;
    recoveryState.reachedRecoveryTarget =
        reachedEof ||
        recoveryState.currentForwardVisibleLeafCount >=
            editWindows[recoveryState.activeEditWindowIndex].forwardTokenCount;
    recoveryState.stableAfterRecovery = reachedEof;
    recoveryState.awaitingStrictStability =
        recoveryState.reachedRecoveryTarget && !recoveryState.stableAfterRecovery;
    recoveryState.strictVisibleLeafCountAfterRecovery = 0;
    recoveryState.currentForwardVisibleLeafCount = 0;
    recoveryState.inRecoveryPhase = false;
    ++recoveryState.activeEditWindowIndex;
    if (recoveryState.activeEditWindowIndex < editWindows.size()) {
      editFloorOffset = editWindows[recoveryState.activeEditWindowIndex].beginOffset;
    }
    ++recoveryStateVersion;
    maybeDisableRecoveryBookkeeping();
  }

  inline void onVisibleLeaf(TextOffset beginOffset, TextOffset endOffset) noexcept {
    if (endOffset <= beginOffset) {
      return;
    }
    if (recoveryState.inRecoveryPhase) {
      const auto *window = currentEditWindow();
      if (window != nullptr && beginOffset >= window->maxCursorOffset) {
        ++recoveryState.currentForwardVisibleLeafCount;
        ++recoveryStateVersion;
      }
      return;
    }
    if (recoveryState.awaitingStrictStability) {
      ++recoveryState.strictVisibleLeafCountAfterRecovery;
      if (recoveryState.strictVisibleLeafCountAfterRecovery >=
          kInternalRecoveryStabilityTokenCount) {
        recoveryState.stableAfterRecovery = true;
        recoveryState.awaitingStrictStability = false;
        ++recoveryStateVersion;
        maybeDisableRecoveryBookkeeping();
        return;
      }
      ++recoveryStateVersion;
    }
  }

  inline void maybeDisableRecoveryBookkeeping() noexcept {
    if (recoveryState.inRecoveryPhase ||
        recoveryState.awaitingStrictStability ||
        recoveryState.activeEditWindowIndex < editWindows.size()) {
      return;
    }
    recoveryState.recoveryBookkeepingEnabled = false;
    ++recoveryStateVersion;
    _recordFailureHistory = false;
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

} // namespace pegium::parser
