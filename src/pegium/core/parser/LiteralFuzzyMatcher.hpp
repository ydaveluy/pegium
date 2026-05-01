#pragma once

/// Fuzzy literal matching utilities used during recovery.

#include <array>
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

/// Direct-mapped cache for `find_literal_fuzzy_candidates`.
///
/// Memoizes the Levenshtein-DP keyed on the (literal storage, input span,
/// case-sensitivity) identity tuple. Both pointers are stable for the
/// duration of a single parse: literal storage lives in the grammar, and the
/// input view points into the immutable text snapshot. The cache must
/// therefore live no longer than one parse — embed it in the context that
/// owns the parse, do not promote it to a thread-local or global. The slot
/// count balances hit rate against L2 footprint; doubling from 64 to 256
/// halved the dominant pathological-recovery sample without measurable
/// regression elsewhere. On collision we evict the older entry.
struct LiteralFuzzyCandidatesCache {
  struct Entry {
    const char *literalData = nullptr;
    std::size_t literalSize = 0;
    const char *inputData = nullptr;
    std::size_t inputSize = 0;
    bool caseSensitive = false;
    LiteralFuzzyCandidates result;
  };
  static constexpr std::size_t kSlotCount = 256;
  std::array<Entry, kSlotCount> slots{};
};

[[nodiscard]] LiteralFuzzyCandidates
find_literal_fuzzy_candidates(std::string_view literal, std::string_view input,
                              bool caseSensitive,
                              LiteralFuzzyCandidatesCache *cache = nullptr) noexcept;

[[nodiscard]] std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept;

} // namespace pegium::parser::detail
