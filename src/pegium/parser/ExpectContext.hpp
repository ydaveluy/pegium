#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <pegium/parser/Parser.hpp>
#include <pegium/parser/ContextShared.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <span>
#include <pegium/utils/Cancellation.hpp>
#include <utility>
#include <vector>

namespace pegium::parser {

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
    using RestoreFn = void (*)(ExpectContext &, const void *) noexcept;

    ScopeGuard(ExpectContext &ctx, RestoreFn restore,
               const void *previous) noexcept
        : _ctx(ctx), _restore(restore), _previous(previous) {}

    ExpectContext &_ctx;
    RestoreFn _restore = nullptr;
    const void *_previous = nullptr;
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
        .editState = detail::capture_edit_checkpoint(*this),
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
    detail::restore_edit_checkpoint(*this, checkpoint.editState);
  }

  [[nodiscard]] SkipperGuard with_skipper(const Skipper &overrideSkipper) noexcept {
    return SkipperGuard{*this, _skipper, overrideSkipper};
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

  [[nodiscard]] constexpr NodeCount node_count() const noexcept { return 0; }

  void override_grammar_element(NodeId,
                                const grammar::AbstractElement *) noexcept {}

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

  constexpr void
  setMaxConsecutiveCodepointDeletes(std::uint32_t maxDeletes) noexcept {
    maxConsecutiveCodepointDeletes = maxDeletes;
  }

  constexpr void setMaxEditsPerAttempt(std::uint32_t maxEdits) noexcept {
    maxEditsPerAttempt = maxEdits;
  }

  constexpr void setMaxEditCost(std::uint32_t maxCost) noexcept {
    maxEditCost = maxCost;
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
    return detail::is_active_recovery(_activeRecoveries, *this, element);
  }

  [[nodiscard]] ActiveRecoveryGuard
  enterActiveRecovery(const grammar::AbstractElement *element) noexcept {
    return detail::enter_active_recovery(_activeRecoveries, *this, element);
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

  bool insertHidden(const grammar::AbstractElement *) {
    if (!trackEditState || !canInsert() ||
        !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      return false;
    }
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted), editCost,
        editCount, hadEdits, consecutiveDeletes);
    return true;
  }

  bool insertHiddenGapAt(const char *position) {
    if (!trackEditState || position < begin || position > anchor) {
      return false;
    }
    if (const auto offset = static_cast<TextOffset>(position - begin);
        !detail::can_insert(allowInsert, canEditAtOffset(offset)) ||
        !canAffordEdit(ParseDiagnosticKind::Inserted)) {
      return false;
    }
    detail::apply_insert_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Inserted), editCost,
        editCount, hadEdits, consecutiveDeletes);
    return true;
  }

  bool deleteOneCodepoint() noexcept {
    if (!trackEditState || !canDelete() ||
        !canAffordEdit(ParseDiagnosticKind::Deleted)) {
      return false;
    }
    const char *const next = detail::next_codepoint_cursor(cursor());
    if (next <= cursor() || next > anchor) {
      return false;
    }
    _cursor = next;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    detail::apply_delete_edit_state(
        detail::default_edit_cost(ParseDiagnosticKind::Deleted), editCost,
        editCount, hadEdits, consecutiveDeletes);
    skip();
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
    if (!trackEditState || endPtr <= cursor() || endPtr > anchor ||
        !canEdit() || !canAffordEdit(replacementCost)) {
      return false;
    }
    detail::apply_replace_edit_state(replacementCost, editCost, editCount,
                                     hadEdits, consecutiveDeletes);
    leaf(endPtr, element, hidden, true);
    return true;
  }

  [[nodiscard]] bool frontierBlocked() const noexcept { return _frontierBlocked; }

  void clearFrontierBlock() noexcept { _frontierBlocked = false; }

  void blockFrontier() noexcept { _frontierBlocked = true; }

  void addKeyword(const grammar::Literal *literal) {
    addFrontier(makePath(literal));
  }

  void addRule(const grammar::AbstractRule *rule) {
    addFrontier(makePath(rule));
  }

  void addReference(const grammar::Assignment *assignment) {
    addFrontier(makePath(assignment));
  }

  void mergeFrontier(std::span<const ExpectPath> items) {
    if (items.empty()) {
      return;
    }
    if (items.size() == 1) {
      addFrontier(items.front());
      return;
    }
    for (const auto &item : items) {
      addFrontier(item);
    }
  }

  std::vector<ExpectPath> frontier;
  bool allowInsert = true;
  bool allowDelete = true;
  bool trackEditState = true;
  bool inRecoveryPhase = true;
  bool hadEdits = false;
  std::uint32_t consecutiveDeletes = 0;
  std::uint32_t maxConsecutiveCodepointDeletes = 8;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
  TextOffset editFloorOffset = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;

private:
  static void restore_rule(ExpectContext &ctx, const void *previous) noexcept {
    ctx._currentRule = static_cast<const grammar::AbstractRule *>(previous);
    ctx.popContextElement();
  }

  static void restore_assignment(ExpectContext &ctx,
                                 const void *previous) noexcept {
    ctx._currentAssignment =
        static_cast<const grammar::Assignment *>(previous);
    ctx.popContextElement();
  }

  void addFrontier(ExpectPath path) {
    if (!path.isValid()) {
      return;
    }
    if (frontier.empty()) [[likely]] {
      frontier.push_back(std::move(path));
      _frontierBlocked = true;
      return;
    }
    for (const auto &existing : frontier) {
      if (existing.elements.size() != path.elements.size()) {
        continue;
      }
      if (std::ranges::equal(existing.elements, path.elements,
                             sameContextElement)) {
        _frontierBlocked = true;
        return;
      }
    }
    frontier.push_back(std::move(path));
    _frontierBlocked = true;
  }

  [[nodiscard]] ExpectPath
  makePath(const grammar::AbstractElement *leaf) const {
    ExpectPath path{.elements = _contextPath};
    if (leaf != nullptr &&
        (path.elements.empty() || path.elements.back() != leaf)) {
      path.elements.push_back(leaf);
    }
    return path;
  }

  void pushContextElement(const grammar::AbstractElement *element) {
    if (element != nullptr) {
      _contextPath.push_back(element);
    }
  }

  void popContextElement() noexcept {
    if (!_contextPath.empty()) {
      _contextPath.pop_back();
    }
  }

  [[nodiscard]] static bool sameContextElement(
      const grammar::AbstractElement *lhs,
      const grammar::AbstractElement *rhs) noexcept {
    if (lhs == rhs) {
      return true;
    }
    if (lhs == nullptr || rhs == nullptr || lhs->getKind() != rhs->getKind()) {
      return false;
    }
    using enum grammar::ElementKind;
    switch (lhs->getKind()) {
    case Literal:
      return static_cast<const grammar::Literal *>(lhs)->getValue() ==
             static_cast<const grammar::Literal *>(rhs)->getValue();
    case Assignment: {
      const auto *lhsAssignment = static_cast<const grammar::Assignment *>(lhs);
      const auto *rhsAssignment = static_cast<const grammar::Assignment *>(rhs);
      return lhsAssignment->getOperator() == rhsAssignment->getOperator() &&
             lhsAssignment->getFeature() == rhsAssignment->getFeature() &&
             lhsAssignment->isReference() == rhsAssignment->isReference() &&
             lhsAssignment->isMultiReference() ==
                 rhsAssignment->isMultiReference() &&
             lhsAssignment->getReferenceType() ==
                 rhsAssignment->getReferenceType();
    }
    case DataTypeRule:
    case ParserRule:
    case TerminalRule:
    case InfixRule:
      return static_cast<const grammar::AbstractRule *>(lhs)->getName() ==
             static_cast<const grammar::AbstractRule *>(rhs)->getName();
    default:
      return false;
    }
  }

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
