#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>

#include <algorithm>
#include <limits>
#include <vector>

namespace pegium::parser::detail {

namespace {

constexpr std::uint32_t kNoMatch =
    std::numeric_limits<std::uint32_t>::max() / 4u;
constexpr std::uint32_t kInsertionCost = 1u;
constexpr std::uint32_t kDeletionCost = 4u;
constexpr std::uint32_t kSubstitutionCost = 2u;
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

struct DpState {
  DpCell anyEdit;
  DpCell noSubstitution;
};

enum class DpLane : std::uint8_t {
  AnyEdit,
  NoSubstitution,
};

[[nodiscard]] constexpr bool is_better_cell(const DpCell &lhs,
                                            const DpCell &rhs) noexcept {
  if (lhs.weightedCost != rhs.weightedCost) {
    return lhs.weightedCost < rhs.weightedCost;
  }
  return lhs.operations < rhs.operations;
}

void consider_transition(DpCell &best, const DpCell &candidate,
                         ParentOp parent, std::uint32_t operationDelta,
                         std::uint32_t weightDelta) noexcept {
  if (candidate.operations == kNoMatch) {
    return;
  }

  const DpCell next{.operations = candidate.operations + operationDelta,
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

[[nodiscard]] constexpr RecoveryCost
make_fuzzy_recovery_cost(std::size_t literalSize,
                         std::uint32_t rawWeightedCost) noexcept {
  return make_recovery_cost(
      rawWeightedCost,
      ceil_div(rawWeightedCost * kReferenceLiteralLength,
               static_cast<std::uint32_t>(std::max<std::size_t>(literalSize, 1u))),
      rawWeightedCost);
}

[[nodiscard]] LiteralFuzzyCandidate
reconstruct_candidate(const std::vector<DpState> &cells, std::size_t cols,
                      std::string_view literal, std::size_t consumed,
                      const DpCell &cell, DpLane lane) noexcept {
  const auto cell_at =
      [&cells, cols, lane](std::size_t literalIndex,
                           std::size_t consumedIndex) noexcept
      -> const DpCell & {
    const auto &state = cells[literalIndex * cols + consumedIndex];
    return lane == DpLane::AnyEdit ? state.anyEdit : state.noSubstitution;
  };

  LiteralFuzzyCandidate candidate{
      .consumed = consumed,
      .distance = cell.operations,
      .operationCount = cell.operations,
      .rawWeightedCost = cell.weightedCost,
      .cost = make_fuzzy_recovery_cost(literal.size(), cell.weightedCost),
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

  candidate.operationCount = candidate.insertionCount + candidate.deletionCount +
                             candidate.substitutionCount +
                             candidate.transpositionCount;
  return candidate;
}

[[nodiscard]] constexpr bool
same_candidate(const LiteralFuzzyCandidate &lhs,
               const LiteralFuzzyCandidate &rhs) noexcept {
  return lhs.consumed == rhs.consumed && lhs.distance == rhs.distance &&
         lhs.operationCount == rhs.operationCount &&
         lhs.rawWeightedCost == rhs.rawWeightedCost &&
         lhs.cost.budgetCost == rhs.cost.budgetCost &&
         lhs.cost.primaryRankCost == rhs.cost.primaryRankCost &&
         lhs.cost.secondaryRankCost == rhs.cost.secondaryRankCost &&
         lhs.insertionCount == rhs.insertionCount &&
         lhs.deletionCount == rhs.deletionCount &&
         lhs.substitutionCount == rhs.substitutionCount &&
         lhs.transpositionCount == rhs.transpositionCount;
}

[[nodiscard]] constexpr bool
is_better_candidate(const LiteralFuzzyCandidate &lhs,
                    const LiteralFuzzyCandidate &rhs) noexcept {
  if (lhs.cost.primaryRankCost != rhs.cost.primaryRankCost) {
    return lhs.cost.primaryRankCost < rhs.cost.primaryRankCost;
  }
  if (lhs.cost.secondaryRankCost != rhs.cost.secondaryRankCost) {
    return lhs.cost.secondaryRankCost < rhs.cost.secondaryRankCost;
  }
  if (lhs.distance != rhs.distance) {
    return lhs.distance < rhs.distance;
  }
  if (lhs.substitutionCount != rhs.substitutionCount) {
    return lhs.substitutionCount < rhs.substitutionCount;
  }
  if (lhs.consumed != rhs.consumed) {
    return lhs.consumed > rhs.consumed;
  }
  if (lhs.operationCount != rhs.operationCount) {
    return lhs.operationCount < rhs.operationCount;
  }
  return lhs.rawWeightedCost < rhs.rawWeightedCost;
}

[[nodiscard]] constexpr bool
dominates_candidate(const LiteralFuzzyCandidate &lhs,
                    const LiteralFuzzyCandidate &rhs) noexcept {
  const bool noWorse =
      lhs.cost.primaryRankCost <= rhs.cost.primaryRankCost &&
      lhs.cost.secondaryRankCost <= rhs.cost.secondaryRankCost &&
      lhs.distance <= rhs.distance &&
      lhs.substitutionCount <= rhs.substitutionCount &&
      lhs.consumed >= rhs.consumed;
  if (!noWorse) {
    return false;
  }
  return lhs.cost.primaryRankCost < rhs.cost.primaryRankCost ||
         lhs.cost.secondaryRankCost < rhs.cost.secondaryRankCost ||
         lhs.distance < rhs.distance ||
         lhs.substitutionCount < rhs.substitutionCount ||
         lhs.consumed > rhs.consumed;
}

constexpr void push_candidate(LiteralFuzzyCandidates &candidates,
                              const LiteralFuzzyCandidate &candidate) {
  for (const auto &existing : candidates) {
    if (same_candidate(existing, candidate)) {
      return;
    }
  }
  candidates.push_back(candidate);
}

[[nodiscard]] LiteralFuzzyCandidates
prune_dominated_candidates(const LiteralFuzzyCandidates &candidates) {
  LiteralFuzzyCandidates frontier;
  frontier.reserve(candidates.size());
  std::vector<bool> dominated(candidates.size(), false);
  for (std::size_t index = 0u; index < candidates.size(); ++index) {
    for (std::size_t otherIndex = 0u; otherIndex < candidates.size();
         ++otherIndex) {
      if (index == otherIndex) {
        continue;
      }
      if (dominates_candidate(candidates[otherIndex], candidates[index])) {
        dominated[index] = true;
        break;
      }
    }
  }
  for (std::size_t index = 0u; index < candidates.size(); ++index) {
    if (!dominated[index]) {
      push_candidate(frontier, candidates[index]);
    }
  }
  std::ranges::sort(frontier,
                    [](const LiteralFuzzyCandidate &lhs,
                       const LiteralFuzzyCandidate &rhs) noexcept {
                      return is_better_candidate(lhs, rhs);
                    });
  return frontier;
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

LiteralFuzzyCandidates find_literal_fuzzy_candidates(std::string_view literal,
                                                     std::string_view input,
                                                     bool caseSensitive) noexcept {
  LiteralFuzzyCandidates candidates;
  candidates.reserve(input.size() * 2u);
  if (literal.empty() || input.empty()) {
    return candidates;
  }
  if (equals_text(literal, input, caseSensitive)) {
    return candidates;
  }

  const auto rows = literal.size() + 1u;
  const auto cols = input.size() + 1u;
  std::vector<DpState> cells(rows * cols);
  const auto cell_at =
      [&cells, cols](std::size_t literalIndex,
                     std::size_t consumedIndex) noexcept -> DpState & {
    return cells[literalIndex * cols + consumedIndex];
  };
  const auto cell_at_const =
      [&cells, cols](std::size_t literalIndex,
                     std::size_t consumedIndex) noexcept -> const DpState & {
    return cells[literalIndex * cols + consumedIndex];
  };

  cell_at(0u, 0u) = {.anyEdit =
                         {.operations = 0u,
                          .weightedCost = 0u,
                          .parent = ParentOp::None},
                     .noSubstitution =
                         {.operations = 0u,
                          .weightedCost = 0u,
                          .parent = ParentOp::None}};
  for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
    DpCell anyBest;
    consider_transition(anyBest, cell_at_const(0u, consumed - 1u).anyEdit,
                        ParentOp::DeleteUnexpectedInputCodepoint, 1u,
                        kDeletionCost);
    DpCell noSubBest;
    consider_transition(noSubBest,
                        cell_at_const(0u, consumed - 1u).noSubstitution,
                        ParentOp::DeleteUnexpectedInputCodepoint, 1u,
                        kDeletionCost);
    cell_at(0u, consumed) = {.anyEdit = anyBest, .noSubstitution = noSubBest};
  }
  for (std::size_t literalIndex = 1u; literalIndex <= literal.size();
       ++literalIndex) {
    DpCell anyBest;
    consider_transition(anyBest,
                        cell_at_const(literalIndex - 1u, 0u).anyEdit,
                        ParentOp::InsertMissingInputCodepoint, 1u,
                        kInsertionCost);
    DpCell noSubBest;
    consider_transition(noSubBest,
                        cell_at_const(literalIndex - 1u, 0u).noSubstitution,
                        ParentOp::InsertMissingInputCodepoint, 1u,
                        kInsertionCost);
    cell_at(literalIndex, 0u) = {.anyEdit = anyBest,
                                 .noSubstitution = noSubBest};
  }

  for (std::size_t literalIndex = 1u; literalIndex <= literal.size();
       ++literalIndex) {
    for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
      DpCell anyBest;
      DpCell noSubBest;
      const auto literalChar =
          static_cast<unsigned char>(literal[literalIndex - 1u]);
      const auto inputChar =
          static_cast<unsigned char>(input[consumed - 1u]);

      if (equal_codepoint(literalChar, inputChar, caseSensitive)) {
        consider_transition(anyBest,
                            cell_at_const(literalIndex - 1u, consumed - 1u)
                                .anyEdit,
                            ParentOp::Match, 0u, 0u);
        consider_transition(noSubBest,
                            cell_at_const(literalIndex - 1u, consumed - 1u)
                                .noSubstitution,
                            ParentOp::Match, 0u, 0u);
      }

      consider_transition(anyBest,
                          cell_at_const(literalIndex - 1u, consumed - 1u)
                              .anyEdit,
                          ParentOp::Substitute, 1u, kSubstitutionCost);
      consider_transition(anyBest,
                          cell_at_const(literalIndex - 1u, consumed).anyEdit,
                          ParentOp::InsertMissingInputCodepoint, 1u,
                          kInsertionCost);
      consider_transition(noSubBest,
                          cell_at_const(literalIndex - 1u, consumed)
                              .noSubstitution,
                          ParentOp::InsertMissingInputCodepoint, 1u,
                          kInsertionCost);
      consider_transition(anyBest,
                          cell_at_const(literalIndex, consumed - 1u).anyEdit,
                          ParentOp::DeleteUnexpectedInputCodepoint, 1u,
                          kDeletionCost);
      consider_transition(noSubBest,
                          cell_at_const(literalIndex, consumed - 1u)
                              .noSubstitution,
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
        consider_transition(anyBest,
                            cell_at_const(literalIndex - 2u, consumed - 2u)
                                .anyEdit,
                            ParentOp::TransposeAdjacentCodepoints, 1u,
                            kTranspositionCost);
        consider_transition(noSubBest,
                            cell_at_const(literalIndex - 2u, consumed - 2u)
                                .noSubstitution,
                            ParentOp::TransposeAdjacentCodepoints, 1u,
                            kTranspositionCost);
      }

      cell_at(literalIndex, consumed) = {.anyEdit = anyBest,
                                         .noSubstitution = noSubBest};
    }
  }

  const bool wordLikeCandidateSpace =
      is_word_like(literal) && is_word_like(input);
  for (std::size_t consumed = 1u; consumed <= input.size(); ++consumed) {
    const auto collect_candidate =
        [&](const DpCell &cell, DpLane lane) noexcept {
          if (cell.operations == kNoMatch || cell.operations == 0u) {
            return;
          }
          auto candidate =
              reconstruct_candidate(cells, cols, literal, consumed, cell, lane);
          if (wordLikeCandidateSpace && consumed < input.size()) {
            const auto trailing =
                static_cast<std::uint32_t>(input.size() - consumed);
            candidate.distance += trailing;
            candidate.deletionCount += trailing;
            candidate.rawWeightedCost += trailing * kDeletionCost;
            candidate.operationCount += trailing;
            candidate.cost =
                make_fuzzy_recovery_cost(literal.size(), candidate.rawWeightedCost);
          }
          push_candidate(candidates, candidate);
        };
    const auto &state = cell_at_const(literal.size(), consumed);
    collect_candidate(state.anyEdit, DpLane::AnyEdit);
    collect_candidate(state.noSubstitution, DpLane::NoSubstitution);
  }

  return prune_dominated_candidates(candidates);
}

std::optional<LiteralFuzzyCandidate>
find_best_literal_fuzzy_candidate(std::string_view literal,
                                  std::string_view input,
                                  bool caseSensitive) noexcept {
  auto candidates =
      find_literal_fuzzy_candidates(literal, input, caseSensitive);
  if (candidates.empty()) {
    return std::nullopt;
  }
  return candidates.front();
}

} // namespace pegium::parser::detail
