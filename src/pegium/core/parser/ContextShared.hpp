#pragma once

/// Shared parser-context guards and editable-state utilities.

#include <algorithm>
#include <concepts>
#include <cstdint>
#include <limits>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/ParseDiagnosticKind.hpp>
#include <pegium/core/parser/RecoveryConstants.hpp>
#include <pegium/core/parser/RecoveryCost.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/utils/TextUtils.hpp>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <vector>

namespace pegium::parser::detail {

// The identifier-codepoint classifier lives in TextUtils so the parser,
// recovery and editor-facing (token boundary) code share one definition.
using utils::is_identifier_like_codepoint;
using utils::is_identifier_like_codepoint_at;

[[nodiscard]] constexpr bool
is_word_like_terminal(std::string_view value) noexcept {
  if (value.empty()) {
    return false;
  }
  const char *cursor = value.data();
  const char *const end = cursor + value.size();
  while (cursor < end) {
    const auto length = utils::utf8_codepoint_length(*cursor);
    if (length == 0 ||
        length > static_cast<std::size_t>(end - cursor)) {
      return false;
    }
    if (!is_identifier_like_codepoint(utils::decode_utf8_codepoint(cursor))) {
      return false;
    }
    cursor += length;
  }
  return true;
}
/// Increment a counter at construction (already done by the caller),
/// decrement it at destruction. Used to maintain a recursion-depth
/// counter on the recovery context with exception-safe scoping.
template <typename Counter> class ScopedDecrementOnExit {
public:
  explicit ScopedDecrementOnExit(Counter &slot) noexcept : _slot(slot) {}
  ScopedDecrementOnExit(const ScopedDecrementOnExit &) = delete;
  ScopedDecrementOnExit &operator=(const ScopedDecrementOnExit &) = delete;
  ScopedDecrementOnExit(ScopedDecrementOnExit &&) = delete;
  ScopedDecrementOnExit &operator=(ScopedDecrementOnExit &&) = delete;
  ~ScopedDecrementOnExit() noexcept { --_slot; }

private:
  Counter &_slot;
};

template <typename Counter>
ScopedDecrementOnExit(Counter &) -> ScopedDecrementOnExit<Counter>;

/// Save a bool reference at construction, restore it at destruction.
class ScopedBoolOverride {
public:
  ScopedBoolOverride(bool &slot, bool next) noexcept
      : _slot(slot), _saved(slot) {
    _slot = next;
  }
  ScopedBoolOverride(const ScopedBoolOverride &) = delete;
  ScopedBoolOverride &operator=(const ScopedBoolOverride &) = delete;
  ScopedBoolOverride(ScopedBoolOverride &&) = delete;
  ScopedBoolOverride &operator=(ScopedBoolOverride &&) = delete;
  ~ScopedBoolOverride() noexcept { _slot = _saved; }

private:
  bool &_slot;
  bool _saved;
};

/// RAII guard for speculative parser exploration. Captures the recovery
/// checkpoint and the furthest-explored cursor on construction; restores
/// both on destruction unless `commit()` was called.
template <typename Context> class ProbeRestoreScope {
public:
  explicit ProbeRestoreScope(Context &ctx) noexcept
      : _ctx(&ctx), _checkpoint(ctx.mark()),
        _savedFurthestExploredCursor(ctx.maxCursor()) {}

  ProbeRestoreScope(const ProbeRestoreScope &) = delete;
  ProbeRestoreScope &operator=(const ProbeRestoreScope &) = delete;
  ProbeRestoreScope(ProbeRestoreScope &&) = delete;
  ProbeRestoreScope &operator=(ProbeRestoreScope &&) = delete;

  ~ProbeRestoreScope() noexcept {
    if (_ctx == nullptr) {
      return;
    }
    _ctx->rewind(_checkpoint);
    _ctx->restoreMaxCursor(_savedFurthestExploredCursor);
  }

  /// Suppress the restore. The mutations performed inside the guarded
  /// region stay committed to the context.
  void commit() noexcept { _ctx = nullptr; }

private:
  Context *_ctx;
  typename Context::Checkpoint _checkpoint;
  const char *_savedFurthestExploredCursor;
};

template <typename Context>
ProbeRestoreScope(Context &) -> ProbeRestoreScope<Context>;

template <typename Context> class EditStateGuard {
public:
  EditStateGuard(Context &ctx, bool nextAllowInsert, bool nextAllowDelete,
                 bool nextTrackEditState) noexcept
      : _ctx(ctx), _prevAllowInsert(ctx.allowInsert),
        _prevAllowDelete(ctx.allowDelete),
        _prevTrackEditState(ctx.trackEditState) {
    _ctx.allowInsert = nextAllowInsert;
    _ctx.allowDelete = nextAllowDelete;
    _ctx.trackEditState = nextTrackEditState;
  }

  EditStateGuard(const EditStateGuard &) = delete;
  EditStateGuard &operator=(const EditStateGuard &) = delete;

  EditStateGuard(EditStateGuard &&other) noexcept
      : _ctx(other._ctx), _prevAllowInsert(other._prevAllowInsert),
        _prevAllowDelete(other._prevAllowDelete),
        _prevTrackEditState(other._prevTrackEditState),
        _active(other._active) {
    other._active = false;
  }

  ~EditStateGuard() noexcept {
    if (_active) {
      _ctx.trackEditState = _prevTrackEditState;
      _ctx.allowInsert = _prevAllowInsert;
      _ctx.allowDelete = _prevAllowDelete;
    }
  }

private:
  Context &_ctx;
  bool _prevAllowInsert;
  bool _prevAllowDelete;
  bool _prevTrackEditState;
  bool _active = true;
};

template <typename Context> class SkipperGuard {
public:
  SkipperGuard(Context &, const Skipper *&slot,
               const Skipper &overrideSkipper) noexcept
      : _slot(slot), _previous(slot) {
    _slot = &overrideSkipper;
  }

  SkipperGuard(const SkipperGuard &) = delete;
  SkipperGuard &operator=(const SkipperGuard &) = delete;

  SkipperGuard(SkipperGuard &&other) noexcept
      : _slot(other._slot), _previous(other._previous), _active(other._active) {
    other._active = false;
  }

  ~SkipperGuard() noexcept {
    if (_active) {
      _slot = _previous;
    }
  }

private:
  const Skipper *&_slot;
  const Skipper *_previous;
  bool _active = true;
};

[[nodiscard]] constexpr bool can_insert(bool allowInsert,
                                        bool canEdit) noexcept {
  return allowInsert && canEdit;
}

[[nodiscard]] constexpr bool
can_delete(bool allowDelete, bool canEdit, std::uint32_t consecutiveDeletes,
           std::uint32_t maxConsecutiveCodepointDeletes,
           char current) noexcept {
  return allowDelete && canEdit &&
         consecutiveDeletes < maxConsecutiveCodepointDeletes &&
         current != '\0';
}

[[nodiscard]] constexpr bool
can_afford_edit(std::uint32_t editCount, std::uint32_t editCost,
                std::uint32_t maxEditsPerAttempt, std::uint32_t maxEditCost,
                ParseDiagnosticKind kind) noexcept {
  return editCount < maxEditsPerAttempt &&
         editCost + default_edit_cost(kind) <= maxEditCost;
}

[[nodiscard]] constexpr bool
can_afford_edit(std::uint32_t editCount, std::uint32_t editCost,
                std::uint32_t maxEditsPerAttempt, std::uint32_t maxEditCost,
                std::uint32_t customCost) noexcept {
  return editCount < maxEditsPerAttempt &&
         editCost + customCost <= maxEditCost;
}

[[nodiscard]] inline const char *
next_codepoint_cursor(const char *cursor) noexcept {
  return static_cast<unsigned char>(*cursor) < 0x80
             ? cursor + 1
             : utils::advanceOneCodepointLossy(cursor);
}

/// Shared bookkeeping for non-delete edits (insert and replace). Both
/// produce a single edit operation that ends any in-progress delete run.
inline void apply_non_delete_edit_state(
    std::uint32_t cost, std::uint32_t &editCost, std::uint32_t &editCount,
    bool &hadEdits, std::uint32_t &consecutiveDeletes) noexcept {
  editCost += cost;
  ++editCount;
  hadEdits = true;
  consecutiveDeletes = 0;
}

inline void apply_delete_edit_state(std::uint32_t cost,
                                    std::uint32_t &editCost,
                                    std::uint32_t &editCount,
                                    bool &hadEdits,
                                    std::uint32_t &consecutiveDeletes) noexcept {
  editCost += cost;
  if (consecutiveDeletes == 0u) {
    ++editCount;
  }
  hadEdits = true;
  ++consecutiveDeletes;
}

struct ActiveRecoveryStack {
  struct Entry {
    const grammar::AbstractElement *element = nullptr;
    const char *cursor = nullptr;
  };

  template <typename Context> class Guard {
  public:
    Guard(ActiveRecoveryStack &stack, Context &ctx,
          const grammar::AbstractElement *element) noexcept
        : _stack(stack) {
      _stack.push(ctx.cursor(), element);
    }

    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

    Guard(Guard &&other) noexcept
        : _stack(other._stack), _active(other._active) {
      other._active = false;
    }

    ~Guard() noexcept {
      if (_active) {
        _stack.pop();
      }
    }

  private:
    ActiveRecoveryStack &_stack;
    bool _active = true;
  };

  [[nodiscard]] bool contains(const char *cursor,
                              const grammar::AbstractElement *element) const
      noexcept {
    return std::ranges::any_of(entries, [cursor, element](const Entry &entry) {
      return entry.element == element && entry.cursor == cursor;
    });
  }

  [[nodiscard]] std::size_t size() const noexcept { return entries.size(); }

  void push(const char *cursor,
            const grammar::AbstractElement *element) noexcept {
    entries.push_back({.element = element, .cursor = cursor});
  }

  void pop() noexcept { entries.pop_back(); }

  std::vector<Entry> entries;
};

} // namespace pegium::parser::detail
