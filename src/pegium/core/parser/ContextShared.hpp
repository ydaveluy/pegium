#pragma once

/// Shared parser-context guards and editable-state utilities.

#include <algorithm>
#include <cstdint>
#include <limits>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/Parser.hpp>
#include <pegium/core/parser/Skipper.hpp>
#include <pegium/core/parser/TextUtils.hpp>
#include <ranges>
#include <vector>

namespace pegium::parser::detail {

struct EditCheckpointState {
  bool allowInsert = true;
  bool allowDelete = true;
  bool trackEditState = true;
  bool inRecoveryPhase = true;
  bool hadEdits = false;
  std::uint32_t consecutiveDeletes = 0;
  std::uint32_t editCost = 0;
  std::uint32_t editCount = 0;
  std::uint32_t maxConsecutiveCodepointDeletes = 8;
  std::uint32_t maxEditsPerAttempt = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t maxEditCost = std::numeric_limits<std::uint32_t>::max();
};

template <typename Context>
concept FlatEditStateContext =
    requires(const Context &ctx, Context &mutableCtx,
             const EditCheckpointState &state) {
      ctx.allowInsert;
      ctx.allowDelete;
      ctx.trackEditState;
      ctx.inRecoveryPhase;
      ctx.hadEdits;
      ctx.consecutiveDeletes;
      ctx.editCost;
      ctx.editCount;
      ctx.maxConsecutiveCodepointDeletes;
      ctx.maxEditsPerAttempt;
      ctx.maxEditCost;
      mutableCtx.allowInsert = state.allowInsert;
      mutableCtx.allowDelete = state.allowDelete;
    };

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

template <typename Context>
[[nodiscard]] inline EditStateGuard<Context>
make_edit_permissions_guard(Context &ctx, bool nextAllowInsert,
                            bool nextAllowDelete) noexcept {
  return EditStateGuard<Context>(ctx, nextAllowInsert, nextAllowDelete,
                                 ctx.trackEditState);
}

template <typename Context>
[[nodiscard]] inline EditStateGuard<Context>
make_edit_state_guard(Context &ctx, bool nextAllowInsert, bool nextAllowDelete,
                      bool nextTrackEditState) noexcept {
  return EditStateGuard<Context>(ctx, nextAllowInsert, nextAllowDelete,
                                 nextTrackEditState);
}

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

template <FlatEditStateContext Context>
[[nodiscard]] inline EditCheckpointState
capture_edit_checkpoint(const Context &ctx) noexcept {
  return {
      .allowInsert = ctx.allowInsert,
      .allowDelete = ctx.allowDelete,
      .trackEditState = ctx.trackEditState,
      .inRecoveryPhase = ctx.inRecoveryPhase,
      .hadEdits = ctx.hadEdits,
      .consecutiveDeletes = ctx.consecutiveDeletes,
      .editCost = ctx.editCost,
      .editCount = ctx.editCount,
      .maxConsecutiveCodepointDeletes = ctx.maxConsecutiveCodepointDeletes,
      .maxEditsPerAttempt = ctx.maxEditsPerAttempt,
      .maxEditCost = ctx.maxEditCost,
  };
}

template <FlatEditStateContext Context>
inline void restore_edit_checkpoint(Context &ctx,
                                    const EditCheckpointState &state) noexcept {
  ctx.allowInsert = state.allowInsert;
  ctx.allowDelete = state.allowDelete;
  ctx.trackEditState = state.trackEditState;
  ctx.inRecoveryPhase = state.inRecoveryPhase;
  ctx.hadEdits = state.hadEdits;
  ctx.consecutiveDeletes = state.consecutiveDeletes;
  ctx.editCost = state.editCost;
  ctx.editCount = state.editCount;
  ctx.maxConsecutiveCodepointDeletes = state.maxConsecutiveCodepointDeletes;
  ctx.maxEditsPerAttempt = state.maxEditsPerAttempt;
  ctx.maxEditCost = state.maxEditCost;
}

[[nodiscard]] constexpr std::uint32_t
default_edit_cost(ParseDiagnosticKind kind) noexcept {
  using enum ParseDiagnosticKind;
  switch (kind) {
  case Inserted:
    return 1;
  case Replaced:
    return 2;
  case Deleted:
    return 4;
  case Recovered:
    return 8;
  case Incomplete:
    return 16;
  case ConversionError:
    return 0;
  }
  return 16;
}

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
             : advanceOneCodepointLossy(cursor);
}

inline void apply_insert_edit_state(std::uint32_t cost,
                                    std::uint32_t &editCost,
                                    std::uint32_t &editCount,
                                    bool &hadEdits,
                                    std::uint32_t &consecutiveDeletes) noexcept {
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
  ++editCount;
  hadEdits = true;
  ++consecutiveDeletes;
}

inline void apply_replace_edit_state(std::uint32_t cost,
                                     std::uint32_t &editCost,
                                     std::uint32_t &editCount,
                                     bool &hadEdits,
                                     std::uint32_t &consecutiveDeletes) noexcept {
  editCost += cost;
  ++editCount;
  hadEdits = true;
  consecutiveDeletes = 0;
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
      _stack.entries.push_back({.element = element, .cursor = ctx.cursor()});
    }

    Guard(const Guard &) = delete;
    Guard &operator=(const Guard &) = delete;

    Guard(Guard &&other) noexcept
        : _stack(other._stack), _active(other._active) {
      other._active = false;
    }

    ~Guard() noexcept {
      if (_active) {
        _stack.entries.pop_back();
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

  std::vector<Entry> entries;
};

template <typename Context>
[[nodiscard]] inline bool
is_active_recovery(const ActiveRecoveryStack &stack, const Context &ctx,
                   const grammar::AbstractElement *element) noexcept {
  return stack.contains(ctx.cursor(), element);
}

template <typename Context>
[[nodiscard]] inline auto
enter_active_recovery(ActiveRecoveryStack &stack, Context &ctx,
                      const grammar::AbstractElement *element) noexcept {
  return typename ActiveRecoveryStack::template Guard<Context>(stack, ctx,
                                                               element);
}

} // namespace pegium::parser::detail
