#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <vector>

namespace pegium::parser {

struct ParseContext {
  static constexpr std::size_t kDefaultMaxConsecutiveCodepointDeletes = 8;

  struct Checkpoint {
    const char *cursor;
    CstBuilder::Checkpoint builder;
    std::uint32_t consecutiveDeletes;
    std::uint32_t diagnosticCount;
    bool hadEdits;
  };

  const char *const begin;
  const char *const end;

  ParseContext(CstBuilder &builder, const Skipper &skipper) noexcept
      : begin(builder.input_begin()), end(builder.input_end()), _cursor(begin),
        _maxCursor(begin), builder(builder), context(skipper) {}

  bool allowInsert = true;
  bool allowDelete = true;
  bool trackEditState = true;
  bool hadEdits = false;
  std::uint32_t consecutiveDeletes = 0;
  std::uint32_t maxConsecutiveCodepointDeletes =
      kDefaultMaxConsecutiveCodepointDeletes;
  std::uint32_t editFloorOffset = 0;
  std::uint32_t editCeilingOffset = std::numeric_limits<std::uint32_t>::max();
  std::vector<ParseDiagnostic> diagnostics;

  [[nodiscard]] inline Checkpoint mark() const noexcept {
    detail::stepTraceInc(detail::StepCounter::RecoverStateMark);

    return {.cursor = _cursor,
            .builder = builder.mark(),
            .consecutiveDeletes = consecutiveDeletes,
            .diagnosticCount = static_cast<std::uint32_t>(diagnostics.size()),
            .hadEdits = hadEdits};
  }

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    detail::stepTraceInc(detail::StepCounter::RecoverStateRewind);
    builder.rewind(checkpoint.builder);
    _cursor = checkpoint.cursor;
    if (!trackEditState) {
      return;
    }
    if (!hadEdits && !checkpoint.hadEdits && consecutiveDeletes == 0 &&
        checkpoint.consecutiveDeletes == 0 && diagnostics.empty() &&
        checkpoint.diagnosticCount == 0) {
      return;
    }
    hadEdits = checkpoint.hadEdits;
    consecutiveDeletes = checkpoint.consecutiveDeletes;
    diagnostics.resize(checkpoint.diagnosticCount);
  }

  inline void skipHiddenNodes() noexcept {
    _cursor = context.skipHiddenNodes(_cursor, end, builder);
    if (_cursor > _maxCursor) [[likely]] {
      _maxCursor = _cursor;
    }
  }

  [[nodiscard]] inline Checkpoint enter() noexcept {
    detail::stepTraceInc(detail::StepCounter::RecoverStateEnter);
    const auto checkpoint = mark();
    builder.enter();
    return checkpoint;
  }

  inline void exit(const Checkpoint &checkpoint,
                   const grammar::AbstractElement *element) noexcept {
    detail::stepTraceInc(detail::StepCounter::RecoverStateExit);
    builder.exit(checkpoint.cursor, _cursor, element);
    // A successful node boundary should reopen deletion budget for siblings.
    if (trackEditState) [[unlikely]] {
      consecutiveDeletes = 0;
    }
  }

  inline void leaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false, bool recovered = false) {
    detail::stepTraceInc(detail::StepCounter::RecoverStateLeaf);
    builder.leaf(_cursor, endPtr, element, hidden, recovered);
    _cursor = endPtr;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    if (trackEditState)  {
      consecutiveDeletes = 0;
    }
  }

  [[nodiscard]] inline uint64_t node_count() const noexcept {
    return builder.node_count();
  }

  inline void
  override_grammar_element(NodeId id,
                           const grammar::AbstractElement *element) noexcept {
    builder.override_grammar_element(id, element);
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

  constexpr void setEditFloorOffset(std::uint32_t offset) noexcept {
    editFloorOffset = offset;
  }
  constexpr void setEditCeilingOffset(std::uint32_t offset) noexcept {
    editCeilingOffset = offset;
  }
  constexpr void setTrackEditState(bool enabled) noexcept {
    trackEditState = enabled;
  }
  constexpr void
  setMaxConsecutiveCodepointDeletes(std::uint32_t maxDeletes) noexcept {
    maxConsecutiveCodepointDeletes = maxDeletes;
  }

  [[nodiscard]] constexpr std::uint32_t cursorOffset() const noexcept {
    return static_cast<std::uint32_t>(_cursor - begin);
  }

  [[nodiscard]] constexpr std::uint32_t maxCursorOffset() const noexcept {
    return static_cast<std::uint32_t>(_maxCursor - begin);
  }

  [[nodiscard]] constexpr bool canInsert() const noexcept {
    return allowInsert && canEdit();
  }

  [[nodiscard]] constexpr bool isStrictNoEditMode() const noexcept {
    return !allowInsert && !allowDelete;
  }

  [[nodiscard]] constexpr bool
  canEditAtOffset(std::size_t offset) const noexcept {
    return offset >= editFloorOffset && offset <= editCeilingOffset;
  }

  [[nodiscard]] constexpr bool canEdit() const noexcept {
    return canEditAtOffset(cursorOffset());
  }

  [[nodiscard]] constexpr bool canDelete() const noexcept {
    return allowDelete && consecutiveDeletes < maxConsecutiveCodepointDeletes &&
           _cursor < end && canEdit();
  }

  [[nodiscard]] bool canForceInsertExpected(
      const grammar::AbstractElement *element) const noexcept {
    if (allowInsert || !allowDelete || !canEdit()) {
      return false;
    }
    const bool allowed = context.canForceInsert(element, _cursor, end);
    if (allowed) {
      if (element != nullptr &&
          element->getKind() == grammar::ElementKind::Literal) {

        PEGIUM_RECOVERY_TRACE(
            "[rule] force policy allow literal='",
            static_cast<const grammar::Literal *>(element)->getValue(),
            "' offset=", cursorOffset());
      } else if (element != nullptr) {
        PEGIUM_RECOVERY_TRACE("[rule] force policy allow kind=",
                              static_cast<int>(element->getKind()),
                              " offset=", cursorOffset());
      } else {
        PEGIUM_RECOVERY_TRACE(
            "[rule] force policy allow null element offset=",
            cursorOffset());
      }
    }
    return allowed;
  }

  bool insertHidden(const grammar::AbstractElement *element) noexcept {
    if (!trackEditState) {
      return false;
    }
    if (!canInsert()) {
      PEGIUM_RECOVERY_TRACE("[rule] insert blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " ceil=", editCeilingOffset);
      return false;
    }
    diagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                           .offset = cursorOffset(),
                           .element = element});
    detail::stepTraceInc(detail::StepCounter::RecoverStateInsert);
    builder.leaf(_cursor, _cursor, element, true, true);
    PEGIUM_RECOVERY_TRACE("[rule] insert hidden offset=", cursorOffset(),
                          " kind=", static_cast<int>(element->getKind()));
    hadEdits = true;
    consecutiveDeletes = 0;
    return true;
  }

  bool insertHiddenForced(const grammar::AbstractElement *element) noexcept {
    if (!trackEditState) {
      return false;
    }
    if (!canForceInsertExpected(element)) {
      PEGIUM_RECOVERY_TRACE(
          "[rule] force-insert blocked offset=", cursorOffset(),
          " floor=", editFloorOffset, " ceil=", editCeilingOffset);
      return false;
    }
    diagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                           .offset = cursorOffset(),
                           .element = element});
    detail::stepTraceInc(detail::StepCounter::RecoverStateInsertForced);
    builder.leaf(_cursor, _cursor, element, true, true);
    PEGIUM_RECOVERY_TRACE(
        "[rule] force-insert hidden offset=", cursorOffset(),
        " kind=", static_cast<int>(element->getKind()));
    hadEdits = true;
    consecutiveDeletes = 0;
    return true;
  }

  bool deleteOneCodepoint() noexcept {
    if (!trackEditState) {
      return false;
    }
    if (!canDelete()) {
      PEGIUM_RECOVERY_TRACE("[rule] delete blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " ceil=", editCeilingOffset,
                            " consecutive=", consecutiveDeletes, "/",
                            maxConsecutiveCodepointDeletes);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    diagnostics.push_back({.kind = ParseDiagnosticKind::Deleted,
                           .offset = cursorOffset(),
                           .element = nullptr});
    detail::stepTraceInc(detail::StepCounter::RecoverStateDelete);

    const char *const next = advanceOneCodepointLossy(_cursor, end);
    if (next <= _cursor || next > end) [[unlikely]] {
      return false;
    }

    _cursor = next;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    hadEdits = true;
    ++consecutiveDeletes;
    _cursor = context.skipHiddenNodes(_cursor, end, builder);
    PEGIUM_RECOVERY_TRACE("[rule] delete offset=", beforeOffset, " -> ",
                          cursorOffset());
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    return true;
  }

  bool replaceLeaf(const char *endPtr, const grammar::AbstractElement *element,
                   bool hidden = false) noexcept {
    if (!trackEditState) {
      return false;
    }
    if (endPtr <= _cursor || endPtr > end) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " ceil=", editCeilingOffset);
      return false;
    }
    const auto endOffset = static_cast<std::size_t>(endPtr - begin);
    if (!canEdit() || !canEditAtOffset(endOffset)) {
      PEGIUM_RECOVERY_TRACE("[rule] replace blocked offset=", cursorOffset(),
                            " floor=", editFloorOffset,
                            " ceil=", editCeilingOffset);
      return false;
    }
    const auto beforeOffset = cursorOffset();
    diagnostics.push_back({.kind = ParseDiagnosticKind::Replaced,
                           .offset = cursorOffset(),
                           .element = element});
    detail::stepTraceInc(detail::StepCounter::RecoverStateReplace);
    builder.leaf(_cursor, endPtr, element, hidden, true);
    _cursor = endPtr;
    if (_cursor > _maxCursor) {
      _maxCursor = _cursor;
    }
    hadEdits = true;
    consecutiveDeletes = 0;
    PEGIUM_RECOVERY_TRACE("[rule] replace offset=", beforeOffset, " -> ",
                          cursorOffset(),
                          " kind=", static_cast<int>(element->getKind()));
    return true;
  }

private:
  const char *_cursor;
  const char *_maxCursor;
  CstBuilder &builder;
  const Skipper &context;
};

} // namespace pegium::parser
