#pragma once

/// Fuzzy literal matching utilities used during recovery.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <pegium/core/parser/RecoveryCost.hpp>
#include <string_view>
#include <vector>

namespace pegium::parser::detail {

struct LiteralFuzzyCandidate {
  std::size_t consumed = 0;
  std::uint32_t distance = 0;
  std::uint32_t operationCount = 0;
  std::uint32_t rawWeightedCost = 0;
  RecoveryCost cost{};
  std::uint32_t insertionCount = 0;
  std::uint32_t deletionCount = 0;
  std::uint32_t substitutionCount = 0;
  std::uint32_t transpositionCount = 0;
};

using LiteralFuzzyCandidates = std::vector<LiteralFuzzyCandidate>;

[[nodiscard]] LiteralFuzzyCandidates
find_literal_fuzzy_candidates(std::string_view literal, std::string_view input,
                              bool caseSensitive) noexcept;

[[nodiscard]] std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept;

} // namespace pegium::parser::detail
