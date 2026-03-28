#pragma once

/// Recovery cost types shared by terminal recovery ranking and budgeting.

#include <cstdint>
#include <limits>

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

[[nodiscard]] constexpr std::uint32_t
saturating_add(std::uint32_t lhs, std::uint32_t rhs) noexcept {
  if (lhs > std::numeric_limits<std::uint32_t>::max() - rhs) {
    return std::numeric_limits<std::uint32_t>::max();
  }
  return lhs + rhs;
}

} // namespace pegium::parser::detail
