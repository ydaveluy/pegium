#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/utils/TextUtils.hpp>

namespace pegium::parser::detail {

namespace {

constexpr std::uint32_t kNoMatch =
    std::numeric_limits<std::uint32_t>::max() / 4u;
constexpr std::uint32_t kInsertionCost =
    default_edit_cost(ParseDiagnosticKind::Inserted);
constexpr std::uint32_t kDeletionCost =
    default_edit_cost(ParseDiagnosticKind::Deleted);
constexpr std::uint32_t kSubstitutionCost =
    default_edit_cost(ParseDiagnosticKind::Replaced);
constexpr std::uint32_t kTranspositionCost = kSubstitutionCost;
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
  return lhs.cost.primaryRankCost < rhs.cost.primaryRankCost;
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
  return caseSensitive ? value
                       : static_cast<unsigned char>(
                             pegium::parser::tolower(static_cast<char>(value)));
}

[[nodiscard]] constexpr bool
equal_codepoint(unsigned char lhs, unsigned char rhs,
                bool caseSensitive) noexcept {
  return normalize_char(lhs, caseSensitive) ==
         normalize_char(rhs, caseSensitive);
}

[[nodiscard]] bool is_word_like(std::string_view value) noexcept {
  return is_word_like_terminal(value);
}

[[nodiscard]] bool
input_window_has_non_identifier_codepoint(std::string_view input,
                                          std::size_t windowBytes) noexcept {
  const char *cursor = input.data();
  const char *const end = cursor + std::min(input.size(), windowBytes);
  while (cursor < end) {
    const auto length = utf8_codepoint_length(*cursor);
    if (length == 0 ||
        length > static_cast<std::size_t>(end - cursor)) {
      // Truncated / invalid UTF-8 in the inspected window: keep the same
      // "treat as identifier-like" heuristic the byte-level predecessor
      // applied so adversarial inputs don't lose the matcher fast-path.
      // Invalid bytes can't be decoded into a codepoint anyway; we
      // defer to the literal-vs-input shared-codepoint scan downstream.
      return false;
    }
    if (!is_identifier_like_codepoint(decode_utf8_codepoint(cursor))) {
      return true;
    }
    cursor += length;
  }
  return false;
}

[[nodiscard]] bool
has_word_like_local_anchor(std::string_view literal, std::string_view input,
                           bool caseSensitive) noexcept {
  if (!is_word_like(literal)) {
    return true;
  }

  constexpr std::size_t kAnchorWindow = 3u;
  const auto literalWindow = std::min(literal.size(), kAnchorWindow);
  const auto inputWindow = std::min(input.size(), kAnchorWindow);
  if (literalWindow <= 1u || inputWindow <= 1u) {
    return true;
  }

  // A multi-codepoint UTF-8 window may have continuation bytes that look
  // word-like at the byte level but encode punctuation. Decode codepoints
  // so we don't accidentally classify e.g. `«` (U+00AB) as an anchor
  // letter.
  if (input_window_has_non_identifier_codepoint(input, kAnchorWindow)) {
    return true;
  }

  for (std::size_t literalIndex = 0u; literalIndex < literalWindow;
       ++literalIndex) {
    for (std::size_t inputIndex = 0u; inputIndex < inputWindow; ++inputIndex) {
      const auto indexDistance =
          literalIndex > inputIndex ? literalIndex - inputIndex
                                    : inputIndex - literalIndex;
      if (indexDistance > 1u) {
        continue;
      }
      if (equal_codepoint(static_cast<unsigned char>(literal[literalIndex]),
                          static_cast<unsigned char>(input[inputIndex]),
                          caseSensitive)) {
        return true;
      }
    }
  }
  return false;
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

LiteralFuzzyCandidates
find_literal_fuzzy_candidates(std::string_view literal,
                              std::string_view input, bool caseSensitive,
                              LiteralFuzzyCandidatesCache *cache) noexcept {
  LiteralFuzzyCandidates candidates;
  if (literal.empty() || input.empty()) {
    return candidates;
  }
  if (equals_text(literal, input, caseSensitive)) {
    return candidates;
  }

  // Direct-mapped lookup. The function is called hundreds of thousands of
  // times during pathological recoveries (git-conflict markers, large fuzz
  // sweeps) with substantial input-window repetition: backtracking re-
  // evaluates the same (literal, source-window) tuple as the parser explores
  // alternative branches. We compare by pointer identity — both literal
  // storage (grammar) and input span (text snapshot) are stable for the
  // duration of one parse, so the cache must live in the parse-scoped owner
  // (`RecoveryContext`). It also stores locally-pruned misses, since those
  // dominate invalid-byte adversarial inputs. When the caller does not provide
  // a cache, we fall through and recompute.
  LiteralFuzzyCandidatesCache::Entry *slot = nullptr;
  if (cache != nullptr) {
    const auto literalHash =
        reinterpret_cast<std::uintptr_t>(literal.data()) ^
        (literal.size() * 0x9E3779B97F4A7C15ULL);
    const auto inputHash =
        reinterpret_cast<std::uintptr_t>(input.data()) ^
        (input.size() * 0xBF58476D1CE4E5B9ULL);
    const auto slotIdx =
        (literalHash ^ inputHash ^ (caseSensitive ? 0xDEADBEEFULL : 0u)) %
        LiteralFuzzyCandidatesCache::kSlotCount;
    slot = &cache->slots[slotIdx];
    if (slot->literalData == literal.data() &&
        slot->literalSize == literal.size() &&
        slot->inputData == input.data() && slot->inputSize == input.size() &&
        slot->caseSensitive == caseSensitive) {
      return slot->result;
    }
  }
  const auto store_cache_result = [&](const LiteralFuzzyCandidates &result) {
    if (slot != nullptr) {
      slot->literalData = literal.data();
      slot->literalSize = literal.size();
      slot->inputData = input.data();
      slot->inputSize = input.size();
      slot->caseSensitive = caseSensitive;
      slot->result = result;
    }
  };

  if (!has_word_like_local_anchor(literal, input, caseSensitive)) {
    store_cache_result(candidates);
    return candidates;
  }

  candidates.reserve(input.size() * 2u);
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

  auto pruned = prune_dominated_candidates(candidates);
  store_cache_result(pruned);
  return pruned;
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

bool literal_has_single_edit_strict_match(std::string_view literal,
                                          std::string_view window,
                                          bool caseSensitive) noexcept {
  const auto N = literal.size();
  const auto W = window.size();
  if (N == 0u) {
    return false;
  }
  const auto try_alignment = [&](std::size_t K, int diff) noexcept {
    if (K > W) {
      return false;
    }
    std::size_t p = 0u;
    const std::size_t pmax = std::min(N, K);
    while (p < pmax && equal_codepoint(static_cast<unsigned char>(literal[p]),
                                       static_cast<unsigned char>(window[p]),
                                       caseSensitive)) {
      ++p;
    }
    std::size_t s = 0u;
    const std::size_t smax = std::min(N - p, K - p);
    while (s < smax &&
           equal_codepoint(
               static_cast<unsigned char>(literal[N - 1u - s]),
               static_cast<unsigned char>(window[K - 1u - s]),
               caseSensitive)) {
      ++s;
    }
    const std::size_t litRemain = N - p - s;
    const std::size_t winRemain = K - p - s;
    if (diff == 0 && litRemain == 1u && winRemain == 1u) {
      return true; // substitution
    }
    if (diff == 1 && litRemain == 0u && winRemain == 1u) {
      return true; // insertion in window
    }
    if (diff == -1 && litRemain == 1u && winRemain == 0u) {
      return true; // deletion from window
    }
    if (diff == 0 && litRemain == 2u && winRemain == 2u && N >= 2u &&
        equal_codepoint(static_cast<unsigned char>(literal[p]),
                        static_cast<unsigned char>(window[p + 1u]),
                        caseSensitive) &&
        equal_codepoint(static_cast<unsigned char>(literal[p + 1u]),
                        static_cast<unsigned char>(window[p]),
                        caseSensitive)) {
      return true; // adjacent transposition
    }
    return false;
  };
  if (try_alignment(N, 0)) {
    return true;
  }
  if (W >= N + 1u && try_alignment(N + 1u, 1)) {
    return true;
  }
  if (N >= 1u && W + 1u >= N && try_alignment(N - 1u, -1)) {
    return true;
  }
  return false;
}

} // namespace pegium::parser::detail
