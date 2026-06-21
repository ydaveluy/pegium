#pragma once

/// Expectation-tracing parse context used for completion and diagnostics.

#include <cstddef>
#include <cstdint>
#include <limits>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/parser/RecoveryConstants.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <span>
#include <pegium/core/utils/Cancellation.hpp>
#include <utility>
#include <vector>

namespace pegium::parser {

namespace detail {

/// Snapshot of the inline edit-state fields owned by `ExpectContext`. Only
/// `ExpectContext` keeps these as flat members (recovery's edit budget
/// lives nested inside `RecoveryContext::recoveryState.editBudget`); this
/// type and its capture / restore helpers therefore live in the expect
/// context's own header.
struct EditCheckpointState {
  bool allowInsert = true;
  bool allowDelete = true;
  bool trackEditState = true;
  bool inRecoveryPhase = true;
  bool hadEdits = false;
  std::uint32_t consecutiveDeletes = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
};


} // namespace detail

// ExpectContext drives frontier exploration for completion and expectation.
// Unlike ParseContext, it is not just a cursor/CST holder: it tracks frontier
// candidates, branch blocking, and editable traversal state.
struct ExpectContext {
  struct Checkpoint {
    const char *cursor = nullptr;
    const char *maxCursor = nullptr;
    const Skipper *skipper = nullptr;
    const grammar::AbstractRule *currentRule = nullptr;
    const grammar::Assignment *currentAssignment = nullptr;
    std::size_t contextPathSize = 0;
    std::size_t frontierSize = 0;
    bool frontierBlocked = false;
    detail::EditCheckpointState editState;
  };

  using EditStateGuard = detail::EditStateGuard<ExpectContext>;
  using SkipperGuard = detail::SkipperGuard<ExpectContext>;
  struct ScopeGuard {
    ScopeGuard(const ScopeGuard &) = delete;
    ScopeGuard &operator=(const ScopeGuard &) = delete;
    ScopeGuard(ScopeGuard &&other) noexcept
        : _ctx(other._ctx), _restore(other._restore), _previous(other._previous),
          _active(other._active) {
      other._active = false;
    }

    ~ScopeGuard() noexcept {
      if (_active) {
        _restore(_ctx, _previous);
      }
    }

  private:
    friend struct ExpectContext;
    using RestoreFn =
        void (*)(ExpectContext &, const grammar::AbstractElement *) noexcept;

    ScopeGuard(ExpectContext &ctx, RestoreFn restore,
               const grammar::AbstractElement *previous) noexcept
        : _ctx(ctx), _restore(restore), _previous(previous) {}

    ExpectContext &_ctx;
    RestoreFn _restore = nullptr;
    const grammar::AbstractElement *_previous = nullptr;
    bool _active = true;
  };

  using ActiveRecoveryGuard = detail::ActiveRecoveryStack::Guard<ExpectContext>;

  const char *const begin;
  const char *const end;
  const char *const anchor;

  ExpectContext(std::string_view text, const Skipper &skipper,
                TextOffset anchorOffset,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) noexcept
      : begin(text.data()), end(text.data() + text.size()),
        anchor(text.data() +
               std::min<TextOffset>(anchorOffset,
                                    static_cast<TextOffset>(text.size()))),
        _cursor(begin), _maxCursor(begin), _skipper(&skipper),
        _cancelToken(cancelToken) {}

  [[nodiscard]] detail::EditCheckpointState captureEditState() const noexcept {
    return {
        .allowInsert = allowInsert,
        .allowDelete = allowDelete,
        .trackEditState = trackEditState,
        .inRecoveryPhase = inRecoveryPhase,
        .hadEdits = hadEdits,
        .consecutiveDeletes = consecutiveDeletes,
        .editCost = editCost,
        .editCount = editCount,
        .maxConsecutiveCodepointDeletes = maxConsecutiveCodepointDeletes,
        .maxEditsPerAttempt = maxEditsPerAttempt,
        .maxEditCost = maxEditCost,
    };
  }

  void restoreEditState(const detail::EditCheckpointState &state) noexcept {
    allowInsert = state.allowInsert;
    allowDelete = state.allowDelete;
    trackEditState = state.trackEditState;
    inRecoveryPhase = state.inRecoveryPhase;
    hadEdits = state.hadEdits;
    consecutiveDeletes = state.consecutiveDeletes;
    editCost = state.editCost;
    editCount = state.editCount;
    maxConsecutiveCodepointDeletes = state.maxConsecutiveCodepointDeletes;
    maxEditsPerAttempt = state.maxEditsPerAttempt;
    maxEditCost = state.maxEditCost;
  }

  [[nodiscard]] Checkpoint mark() const noexcept {
    return {
        .cursor = _cursor,
        .maxCursor = _maxCursor,
        .skipper = _skipper,
        .currentRule = _currentRule,
        .currentAssignment = _currentAssignment,
        .contextPathSize = _contextPath.size(),
        .frontierSize = frontier.size(),
        .frontierBlocked = _frontierBlocked,
        .editState = captureEditState(),
    };
  }

  void rewind(const Checkpoint &checkpoint) noexcept {
    _cursor = checkpoint.cursor;
    _maxCursor = checkpoint.maxCursor;
    _skipper = checkpoint.skipper;
    _currentRule = checkpoint.currentRule;
    _currentAssignment = checkpoint.currentAssignment;
    _contextPath.resize(checkpoint.contextPathSize);
    frontier.resize(checkpoint.frontierSize);
    _frontierBlocked = checkpoint.frontierBlocked;
    restoreEditState(checkpoint.editState);
  }

  [[nodiscard]] SkipperGuard with_skipper(const Skipper &overrideSkipper) noexcept {
    return SkipperGuard{*this, _skipper, overrideSkipper};
  }

  [[nodiscard]] EditStateGuard
  withEditPermissions(bool nextAllowInsert, bool nextAllowDelete) noexcept {
    return EditStateGuard(*this, nextAllowInsert, nextAllowDelete,
                          trackEditState);
  }

  [[nodiscard]] EditStateGuard withEditTrackingDisabled() noexcept {
    return EditStateGuard(*this, false, false, false);
  }

  [[nodiscard]] ScopeGuard
  with_rule(const grammar::AbstractRule *rule) noexcept {
    const auto *previous = _currentRule;
    _currentRule = rule;
    pushContextElement(rule);
    return ScopeGuard{*this, &restore_rule, previous};
  }

  [[nodiscard]] ScopeGuard
  with_assignment(const grammar::Assignment *assignment) noexcept {
    const auto *previous = _currentAssignment;
    _currentAssignment = assignment;
    pushContextElement(assignment);
    return ScopeGuard{*this, &restore_assignment, previous};
  }

  void skip() noexcept {
    _cursor = _skipper->skip(_cursor);
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
  }

  [[nodiscard]] const char *enter() const noexcept { return _cursor; }

  void exit(const char *, const grammar::AbstractElement *) const {
    utils::throw_if_cancelled(_cancelToken);
  }

  void leaf(const char *endPtr, const grammar::AbstractElement *,
            bool = false, bool = false) {
    _cursor = endPtr;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
  }

  void leaf(const char *, const char *endPtr, const grammar::AbstractElement *,
            bool = false, bool = false) {
    _cursor = endPtr;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
  }

  [[nodiscard]] constexpr NodeCount node_count() const noexcept { return 0; }

  void override_grammar_element(NodeId,
                                const grammar::AbstractElement *) const noexcept {}

  [[nodiscard]] constexpr const char *cursor() const noexcept { return _cursor; }
  [[nodiscard]] constexpr const char *maxCursor() const noexcept {
    return _maxCursor;
  }
  [[nodiscard]] const Skipper &skipper() const noexcept { return *_skipper; }

  constexpr void restoreMaxCursor(const char *cursorValue) noexcept {
    _maxCursor = cursorValue;
  }

  [[nodiscard]] constexpr TextOffset cursorOffset() const noexcept {
    return static_cast<TextOffset>(_cursor - begin);
  }

  [[nodiscard]] constexpr bool reachedAnchor() const noexcept {
    return _cursor >= anchor;
  }

  [[nodiscard]] constexpr bool canTraverseUntil(const char *endPtr) const
      noexcept {
    return endPtr != nullptr && endPtr <= anchor;
  }

  constexpr void setInRecoveryPhase(bool enabled) noexcept {
    inRecoveryPhase = enabled;
  }

  [[nodiscard]] constexpr bool isInRecoveryPhase() const noexcept {
    return inRecoveryPhase;
  }

  [[nodiscard]] constexpr bool hasPendingRecoveryWindows() const noexcept {
    return false;
  }

  [[nodiscard]] std::size_t activeRecoveryDepth() const noexcept {
    return _activeRecoveries.size();
  }

  [[nodiscard]] bool isActiveRecovery(
      const grammar::AbstractElement *element) const noexcept {
    return _activeRecoveries.contains(cursor(), element);
  }

  [[nodiscard]] ActiveRecoveryGuard
  enterActiveRecovery(const grammar::AbstractElement *element) noexcept {
    return ActiveRecoveryGuard(_activeRecoveries, *this, element);
  }

  [[nodiscard]] constexpr bool canEditAtOffset(TextOffset offset) const noexcept {
    return inRecoveryPhase && trackEditState && offset >= editFloorOffset &&
           offset < static_cast<TextOffset>(anchor - begin);
  }

  [[nodiscard]] constexpr bool canEdit() const noexcept {
    return canEditAtOffset(cursorOffset());
  }

  [[nodiscard]] constexpr bool canInsert() const noexcept {
    return detail::can_insert(allowInsert, canEdit());
  }

  [[nodiscard]] constexpr bool canDelete() const noexcept {
    if (_cursor >= anchor) {
      return false;
    }
    return detail::can_delete(allowDelete, canEdit(), consecutiveDeletes,
                              maxConsecutiveCodepointDeletes, *cursor());
  }

  [[nodiscard]] constexpr std::uint32_t currentEditCost() const noexcept {
    return editCost;
  }

  [[nodiscard]] constexpr std::uint32_t currentEditCount() const noexcept {
    return editCount;
  }

  [[nodiscard]] constexpr std::uint32_t
  editCostDelta(const Checkpoint &checkpoint) const noexcept {
    return editCost - checkpoint.editState.editCost;
  }

  [[nodiscard]] constexpr bool
  canAffordEdit(ParseDiagnosticKind kind) const noexcept {
    return detail::can_afford_edit(editCount, editCost, maxEditsPerAttempt,
                                   maxEditCost, kind);
  }

  [[nodiscard]] constexpr bool
  canAffordEdit(std::uint32_t customCost) const noexcept {
    return detail::can_afford_edit(editCount, editCost, maxEditsPerAttempt,
                                   maxEditCost, customCost);
  }

  bool insertSynthetic(const grammar::AbstractElement *);

  bool insertSyntheticGapAt(const char *position,
                            const char *message = nullptr);

  bool deleteOneCodepoint() noexcept;

  // `fuzzyOperationDistance` is accepted for signature parity with
  // `RecoveryContext::replaceLeaf` (whose out-of-window carve-out reads it).
  // ExpectContext has no such carve-out and ignores it.
  bool replaceLeaf(const char *endPtr, const grammar::AbstractElement *element,
                   std::uint32_t replacementCost, bool hidden = false,
                   std::uint32_t fuzzyOperationDistance =
                       std::numeric_limits<std::uint32_t>::max());

  [[nodiscard]] bool frontierBlocked() const noexcept { return _frontierBlocked; }

  void clearFrontierBlock() noexcept { _frontierBlocked = false; }

  void blockFrontier() noexcept { _frontierBlocked = true; }

  void addKeyword(const grammar::Literal *literal);

  void addRule(const grammar::AbstractRule *rule);

  void addReference(const grammar::Assignment *assignment);

  void mergeFrontier(std::span<const ExpectPath> items);

  std::vector<ExpectPath> frontier;
  bool allowInsert = true;
  bool allowDelete = true;
  bool trackEditState = true;
  bool inRecoveryPhase = true;
  bool hadEdits = false;
  std::uint32_t consecutiveDeletes = 0;
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
  TextOffset editFloorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;

private:
  static void restore_rule(ExpectContext &ctx,
                           const grammar::AbstractElement *previous) noexcept;

  static void restore_assignment(
      ExpectContext &ctx,
      const grammar::AbstractElement *previous) noexcept;

  void addFrontier(ExpectPath path);

  [[nodiscard]] ExpectPath
  makePath(const grammar::AbstractElement *leaf) const;

  void pushContextElement(const grammar::AbstractElement *element);

  void popContextElement() noexcept;

  [[nodiscard]] bool sameContextElement(
      const grammar::AbstractElement *lhs,
      const grammar::AbstractElement *rhs) const noexcept;

  const char *_cursor = nullptr;
  const char *_maxCursor = nullptr;
  const Skipper *_skipper = nullptr;
  const utils::CancellationToken &_cancelToken;
  const grammar::AbstractRule *_currentRule = nullptr;
  const grammar::Assignment *_currentAssignment = nullptr;
  std::vector<const grammar::AbstractElement *> _contextPath;
  detail::ActiveRecoveryStack _activeRecoveries;
  bool _frontierBlocked = false;
};

} // namespace pegium::parser
