#pragma once

/// Helpers for applying recovery edits in editable parse modes.

#include <cstdint>

#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>

namespace pegium::parser::detail {

template <EditableParseModeContext Context>
[[nodiscard]] constexpr bool
can_apply_recovery_match(const Context &ctx, const char *endPtr) noexcept {
  if constexpr (RecoveryParseModeContext<Context>) {
    (void)ctx;
    return endPtr != nullptr;
  } else {
    return ctx.canTraverseUntil(endPtr);
  }
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] constexpr bool
apply_insert_hidden_recovery_edit(Context &ctx, const Element *element) {
  return ctx.insertHidden(element);
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] constexpr bool
apply_insert_hidden_gap_recovery_edit(Context &ctx, const char *position,
                                      const Element *element) {
  if (!ctx.insertHiddenGapAt(position)) {
    return false;
  }
  ctx.leaf(position, element);
  return true;
}

template <EditableParseModeContext Context, typename Element>
[[nodiscard]] constexpr bool
apply_replace_leaf_recovery_edit(Context &ctx, const char *endPtr,
                                 const Element *element,
                                 std::uint32_t editCost) {
  return ctx.replaceLeaf(endPtr, element, editCost);
}

template <Expression E>
[[nodiscard]] inline bool
attempt_parse_without_side_effects(RecoveryContext &ctx, const E &expression) {
  const auto checkpoint = ctx.mark();
  const auto maxCursor = ctx.maxCursor();
  const bool matched = attempt_parse_no_edits(ctx, expression);
  ctx.rewind(checkpoint);
  ctx.restoreMaxCursor(maxCursor);
  return matched;
}

} // namespace pegium::parser::detail
