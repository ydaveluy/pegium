#pragma once

/// Recovery cost types and the canonical edit-cost table.

#include <cstdint>
#include <limits>

#include <pegium/core/parser/ParseDiagnosticKind.hpp>

namespace pegium::parser::detail {

struct RecoveryCost {
  std::uint32_t budgetCost = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t primaryRankCost = std::numeric_limits<std::uint32_t>::max();
};

/// Saturating ("monus") subtraction: `a - b`, clamped to 0 when `b > a`.
/// Single-sources the `a > b ? a - b : 0` idiom used by the recovery
/// ranking score's two clamps.
[[nodiscard]] constexpr std::uint32_t monus(std::uint32_t a,
                                            std::uint32_t b) noexcept {
  return a > b ? a - b : 0u;
}

[[nodiscard]] constexpr RecoveryCost
make_recovery_cost(std::uint32_t budgetCost,
                   std::uint32_t primaryRankCost) noexcept {
  return {
      .budgetCost = budgetCost,
      .primaryRankCost = primaryRankCost,
  };
}

/// Closed cost table. Adding/changing a cost here propagates through the
/// budget gates (`can_afford_edit`) and the ranking comparator
/// (`is_better_recovery_key` carries `editCost` directly).
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

} // namespace pegium::parser::detail
