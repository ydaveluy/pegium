#pragma once

/// Recovery cost types and the canonical edit-cost table.

#include <cstdint>
#include <limits>

#include <pegium/core/parser/ParseDiagnosticKind.hpp>

namespace pegium::parser::detail {

struct RecoveryCost {
  std::uint32_t budgetCost = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t primaryRankCost = std::numeric_limits<std::uint32_t>::max();
  std::uint32_t secondaryRankCost = std::numeric_limits<std::uint32_t>::max();
};

[[nodiscard]] constexpr RecoveryCost
make_recovery_cost(std::uint32_t budgetCost, std::uint32_t primaryRankCost,
                   std::uint32_t secondaryRankCost) noexcept {
  return {
      .budgetCost = budgetCost,
      .primaryRankCost = primaryRankCost,
      .secondaryRankCost = secondaryRankCost,
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
