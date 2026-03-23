#pragma once

/// Fuzzy literal matching utilities used during recovery.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace pegium::parser::detail {

struct LiteralFuzzyCandidate {
  std::size_t consumed = 0;
  std::uint32_t distance = 0;
  std::uint32_t rawWeightedCost = 0;
  std::uint32_t normalizedEditCost = 0;
  std::uint32_t insertionCount = 0;
  std::uint32_t deletionCount = 0;
  std::uint32_t substitutionCount = 0;
  std::uint32_t transpositionCount = 0;
};

[[nodiscard]] std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept;

[[nodiscard]] constexpr std::uint32_t
literal_fuzzy_replacement_cost(const LiteralFuzzyCandidate &candidate) noexcept {
  return candidate.normalizedEditCost;
}

} // namespace pegium::parser::detail
