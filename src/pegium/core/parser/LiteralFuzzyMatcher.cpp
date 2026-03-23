#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace pegium::parser::detail {

namespace {

constexpr std::uint32_t kNoMatch =
    std::numeric_limits<std::uint32_t>::max() / 4u;
constexpr std::uint32_t kInsertionCost = 2u;
constexpr std::uint32_t kDeletionCost = 2u;
constexpr std::uint32_t kSubstitutionCost = 3u;
constexpr std::uint32_t kTranspositionCost = 2u;
constexpr std::uint32_t kReferenceLiteralLength = 5u;

enum class ParentOp : std::uint8_t {
  None,
  Match,
  Substitute,
  InsertMissingInputCodepoint,
  DeleteUnexpectedInputCodepoint,
  TransposeAdjacentCodepoints,
};

struct DpCell {
  std::uint32_t operations = kNoMatch;
  std::uint32_t weightedCost = kNoMatch;
  ParentOp parent = ParentOp::None;
};

[[nodiscard]] constexpr bool is_better_cell(const DpCell &lhs,
                                            const DpCell &rhs) noexcept {
  if (lhs.operations != rhs.operations) {
    return lhs.operations < rhs.operations;
  }
  return lhs.weightedCost < rhs.weightedCost;
}

void consider_transition(DpCell &best, const DpCell &candidate,
                         ParentOp parent, std::uint32_t opDelta,
                         std::uint32_t weightDelta) noexcept {
  if (candidate.operations == kNoMatch) {
    return;
  }

  const DpCell next{.operations = candidate.operations + opDelta,
                    .weightedCost = candidate.weightedCost + weightDelta,
                    .parent = parent};
  if (best.operations == kNoMatch || is_better_cell(next, best)) {
    best = next;
  }
}

[[nodiscard]] constexpr std::uint32_t
ceil_div(std::uint32_t numerator, std::uint32_t denominator) noexcept {
  return denominator == 0u ? numerator
                           : (numerator + denominator - 1u) / denominator;
}

[[nodiscard]] constexpr std::uint32_t
normalize_fuzzy_edit_cost(std::size_t literalSize,
                          std::uint32_t rawWeightedCost) noexcept {
  const auto normalized = ceil_div(
      rawWeightedCost * kReferenceLiteralLength,
      static_cast<std::uint32_t>(std::max<std::size_t>(literalSize, 1u)));
  return std::max(rawWeightedCost, normalized);
}

[[nodiscard]] LiteralFuzzyCandidate
reconstruct_candidate(const std::vector<DpCell> &cells, std::size_t cols,
                      std::string_view literal, std::size_t consumed,
                      const DpCell &cell) noexcept {
  const auto cell_at =
      [&cells, cols](std::size_t literalIndex,
                     std::size_t consumedIndex) noexcept -> const DpCell & {
    return cells[literalIndex * cols + consumedIndex];
  };

  LiteralFuzzyCandidate candidate{
      .consumed = consumed,
      .distance = cell.operations,
      .rawWeightedCost = cell.weightedCost,
      .normalizedEditCost =
          normalize_fuzzy_edit_cost(literal.size(), cell.weightedCost),
  };

  std::size_t literalIndex = literal.size();
  std::size_t consumedIndex = consumed;
  while (literalIndex > 0u || consumedIndex > 0u) {
    const auto parent = cell_at(literalIndex, consumedIndex).parent;
    switch (parent) {
    case ParentOp::None:
      literalIndex = 0u;
      consumedIndex = 0u;
      break;
    case ParentOp::Match:
      --literalIndex;
      --consumedIndex;
      break;
    case ParentOp::Substitute:
      ++candidate.substitutionCount;
      --literalIndex;
      --consumedIndex;
      break;
    case ParentOp::InsertMissingInputCodepoint:
      ++candidate.insertionCount;
      --literalIndex;
      break;
    case ParentOp::DeleteUnexpectedInputCodepoint:
      ++candidate.deletionCount;
      --consumedIndex;
      break;
    case ParentOp::TransposeAdjacentCodepoints:
      ++candidate.transpositionCount;
      literalIndex -= 2u;
      consumedIndex -= 2u;
      break;
    }
  }

  return candidate;
}

[[nodiscard]] bool is_better_candidate(const LiteralFuzzyCandidate &lhs,
                                       const LiteralFuzzyCandidate &rhs) noexcept {
  if (lhs.normalizedEditCost != rhs.normalizedEditCost) {
    return lhs.normalizedEditCost < rhs.normalizedEditCost;
  }
  if (lhs.distance != rhs.distance) {
    return lhs.distance < rhs.distance;
  }
  if (lhs.consumed != rhs.consumed) {
    return lhs.consumed > rhs.consumed;
  }
  if (lhs.substitutionCount != rhs.substitutionCount) {
    return lhs.substitutionCount < rhs.substitutionCount;
  }
  if (lhs.rawWeightedCost != rhs.rawWeightedCost) {
    return lhs.rawWeightedCost < rhs.rawWeightedCost;
  }
  return lhs.insertionCount + lhs.deletionCount + lhs.transpositionCount <
         rhs.insertionCount + rhs.deletionCount + rhs.transpositionCount;
}

[[nodiscard]] constexpr unsigned char
normalize_char(unsigned char value, bool caseSensitive) noexcept {
  if (!caseSensitive && value >= static_cast<unsigned char>('A') &&
      value <= static_cast<unsigned char>('Z')) {
    return static_cast<unsigned char>(value + ('a' - 'A'));
  }
  return value;
}

[[nodiscard]] constexpr bool
equal_codepoint(unsigned char lhs, unsigned char rhs,
                bool caseSensitive) noexcept {
  return normalize_char(lhs, caseSensitive) ==
         normalize_char(rhs, caseSensitive);
}

[[nodiscard]] constexpr bool is_word_char(unsigned char value) noexcept {
  return (value >= static_cast<unsigned char>('a') &&
          value <= static_cast<unsigned char>('z')) ||
         (value >= static_cast<unsigned char>('A') &&
          value <= static_cast<unsigned char>('Z')) ||
         (value >= static_cast<unsigned char>('0') &&
          value <= static_cast<unsigned char>('9')) ||
         value == static_cast<unsigned char>('_');
}

[[nodiscard]] bool is_word_like(std::string_view value) noexcept {
  return !value.empty() &&
         std::ranges::all_of(value, [](char c) {
           return is_word_char(static_cast<unsigned char>(c));
         });
}

[[nodiscard]] bool equals_text(std::string_view lhs, std::string_view rhs,
                               bool caseSensitive) noexcept {
  if (lhs.size() != rhs.size()) {
    return false;
  }
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    if (!equal_codepoint(static_cast<unsigned char>(lhs[index]),
                         static_cast<unsigned char>(rhs[index]),
                         caseSensitive)) {
      return false;
    }
  }
  return true;
}

} // namespace

std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept {
  if (literal.empty() || input.empty()) {
    return std::nullopt;
  }
  if (equals_text(literal, input, caseSensitive)) {
    return std::nullopt;
  }

  const auto rows = literal.size() + 1u;
  const auto cols = input.size() + 1u;
  std::vector<DpCell> cells(rows * cols);
  const auto cell_at =
      [&cells, cols](std::size_t literalIndex,
                     std::size_t consumedIndex) noexcept -> DpCell & {
    return cells[literalIndex * cols + consumedIndex];
  };
  const auto cell_at_const =
      [&cells, cols](std::size_t literalIndex,
                     std::size_t consumedIndex) noexcept -> const DpCell & {
    return cells[literalIndex * cols + consumedIndex];
  };

  cell_at(0u, 0u) = {.operations = 0u,
                     .weightedCost = 0u,
                     .parent = ParentOp::None};
  for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
    DpCell best;
    consider_transition(best, cell_at_const(0u, consumed - 1u),
                        ParentOp::DeleteUnexpectedInputCodepoint, 1u,
                        kDeletionCost);
    cell_at(0u, consumed) = best;
  }
  for (std::size_t literalIndex = 1u; literalIndex <= literal.size();
       ++literalIndex) {
    DpCell best;
    consider_transition(best, cell_at_const(literalIndex - 1u, 0u),
                        ParentOp::InsertMissingInputCodepoint, 1u,
                        kInsertionCost);
    cell_at(literalIndex, 0u) = best;
  }

  for (std::size_t literalIndex = 1u; literalIndex <= literal.size();
       ++literalIndex) {
    for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
      DpCell best;
      const auto literalChar =
          static_cast<unsigned char>(literal[literalIndex - 1u]);
      const auto inputChar =
          static_cast<unsigned char>(input[consumed - 1u]);

      if (equal_codepoint(literalChar, inputChar, caseSensitive)) {
        consider_transition(best, cell_at_const(literalIndex - 1u, consumed - 1u),
                            ParentOp::Match, 0u, 0u);
      }

      consider_transition(best, cell_at_const(literalIndex - 1u, consumed - 1u),
                          ParentOp::Substitute, 1u, kSubstitutionCost);
      consider_transition(best, cell_at_const(literalIndex - 1u, consumed),
                          ParentOp::InsertMissingInputCodepoint, 1u,
                          kInsertionCost);
      consider_transition(best, cell_at_const(literalIndex, consumed - 1u),
                          ParentOp::DeleteUnexpectedInputCodepoint, 1u,
                          kDeletionCost);

      if (literalIndex >= 2u && consumed >= 2u &&
          equal_codepoint(
              static_cast<unsigned char>(literal[literalIndex - 1u]),
              static_cast<unsigned char>(input[consumed - 2u]),
              caseSensitive) &&
          equal_codepoint(
              static_cast<unsigned char>(literal[literalIndex - 2u]),
              static_cast<unsigned char>(input[consumed - 1u]),
              caseSensitive)) {
        consider_transition(best, cell_at_const(literalIndex - 2u, consumed - 2u),
                            ParentOp::TransposeAdjacentCodepoints, 1u,
                            kTranspositionCost);
      }

      cell_at(literalIndex, consumed) = best;
    }
  }

  std::optional<LiteralFuzzyCandidate> bestCandidate;
  const bool wordLikeCandidateSpace =
      is_word_like(literal) && is_word_like(input);
  for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
    const auto &cell = cell_at_const(literal.size(), consumed);
    if (cell.operations == kNoMatch || cell.operations == 0u) {
      continue;
    }
    auto candidate = reconstruct_candidate(cells, cols, literal, consumed, cell);
    if (wordLikeCandidateSpace && consumed < input.size()) {
      const auto trailing = static_cast<std::uint32_t>(input.size() - consumed);
      candidate.distance += trailing;
      candidate.rawWeightedCost += trailing * kDeletionCost;
      candidate.deletionCount += trailing;
      candidate.normalizedEditCost =
          normalize_fuzzy_edit_cost(literal.size(), candidate.rawWeightedCost);
    }
    if (!bestCandidate.has_value() ||
        is_better_candidate(candidate, *bestCandidate)) {
      bestCandidate = std::move(candidate);
    }
  }
  return bestCandidate;
}

} // namespace pegium::parser::detail
