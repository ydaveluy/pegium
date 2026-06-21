#include <algorithm>
#include <cstdint>
#include <gtest/gtest.h>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <string>
#include <string_view>

using namespace pegium::parser::detail;

namespace {

const LiteralFuzzyCandidate &
requireCandidate(const std::optional<LiteralFuzzyCandidate> &candidate) {
  EXPECT_TRUE(candidate.has_value());
  return *candidate;
}

const LiteralFuzzyCandidate *
findCandidate(const LiteralFuzzyCandidates &candidates,
              auto &&predicate) noexcept {
  const auto it = std::ranges::find_if(candidates, predicate);
  return it == candidates.end() ? nullptr : std::addressof(*it);
}

// Owning wrappers over the production view contract (the former by-value
// find_literal_fuzzy_candidates / find_best_literal_fuzzy_candidate were
// test-only and have been removed from production).
LiteralFuzzyCandidates allCandidates(std::string_view literal,
                                     std::string_view input,
                                     bool caseSensitive) {
  LiteralFuzzyCandidatesCache cache;
  const auto view =
      find_literal_fuzzy_candidates_view(literal, input, caseSensitive, cache);
  return LiteralFuzzyCandidates(view.begin(), view.end());
}

std::optional<LiteralFuzzyCandidate>
bestCandidate(std::string_view literal, std::string_view input,
              bool caseSensitive) {
  const auto candidates = allCandidates(literal, input, caseSensitive);
  if (candidates.empty()) {
    return std::nullopt;
  }
  return candidates.front();
}

} // namespace

TEST(LiteralFuzzyMatcherTest, SupportsSingleEditOperations) {
  // One distance-1 candidate per edit kind. In every single-edit case here
  // rawWeightedCost == cost.budgetCost == cost.primaryRankCost, so one `cost`
  // column carries all three. Each row asserts the full op-count vector (the
  // active op plus zeros) — strictly stronger than the original per-op tests.
  struct Case {
    const char *name;
    std::string_view literal;
    std::string_view input;
    std::uint32_t consumed;
    std::uint32_t insertionCount;
    std::uint32_t deletionCount;
    std::uint32_t substitutionCount;
    std::uint32_t transpositionCount;
    std::uint32_t cost;
  };
  static constexpr Case kCases[] = {
      {"single deletion from input", "module", "modle", 5u, 1u, 0u, 0u, 0u, 1u},
      {"single insertion in input", "module", "modulee", 7u, 0u, 1u, 0u, 0u, 4u},
      {"single adjacent transposition", "module", "modlue", 6u, 0u, 0u, 0u, 1u,
       2u},
      {"single substitution", "service", "servixe", 7u, 0u, 0u, 1u, 0u, 2u},
  };
  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    const auto candidate =
        bestCandidate(c.literal, c.input, true);
    const auto &match = requireCandidate(candidate);
    EXPECT_EQ(match.consumed, c.consumed);
    EXPECT_EQ(match.distance, 1u);
    EXPECT_EQ(match.insertionCount, c.insertionCount);
    EXPECT_EQ(match.deletionCount, c.deletionCount);
    EXPECT_EQ(match.substitutionCount, c.substitutionCount);
    EXPECT_EQ(match.transpositionCount, c.transpositionCount);
    EXPECT_EQ(match.rawWeightedCost, c.cost);
    EXPECT_EQ(match.cost.budgetCost, c.cost);
    EXPECT_EQ(match.cost.primaryRankCost, c.cost);
  }
}

TEST(LiteralFuzzyMatcherTest,
     RejectsWordLikeWindowsWithoutLocalPrefixAnchor) {
  const auto candidates =
      allCandidates("extends", "tring", false);

  EXPECT_TRUE(candidates.empty());
}

TEST(LiteralFuzzyMatcherTest,
     KeepsWordLikeFirstCodepointSubstitutionWithLocalAnchor) {
  const auto candidate =
      bestCandidate("entity", "xntity", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 6u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.substitutionCount, 1u);
}

TEST(LiteralFuzzyMatcherTest,
     RejectsInvalidUtf8WordWindowWithoutLocalPrefixAnchor) {
  std::string input = "unsa";
  input.push_back(static_cast<char>(0x96));
  input += "fe";

  LiteralFuzzyCandidatesCache cache;
  const auto candidates =
      find_literal_fuzzy_candidates_view("export", input, false, cache);
  const auto cachedCandidates =
      find_literal_fuzzy_candidates_view("export", input, false, cache);

  EXPECT_TRUE(candidates.empty());
  EXPECT_TRUE(cachedCandidates.empty());
}

TEST(LiteralFuzzyMatcherTest,
     KeepsInvalidUtf8WordWindowWithLocalPrefixAnchor) {
  std::string input = "unsa";
  input.push_back(static_cast<char>(0x96));
  input += "fe";

  const auto candidate =
      bestCandidate("unsafe", input, false);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 4u);
  EXPECT_EQ(match.distance, 2u);
  EXPECT_EQ(match.insertionCount, 2u);
  EXPECT_EQ(match.deletionCount, 0u);
}

TEST(LiteralFuzzyMatcherTest, ExactCaseInsensitiveMatchDoesNotRequireFuzzyEdit) {
  const auto candidate =
      bestCandidate("module", "ModUle", false);

  EXPECT_FALSE(candidate.has_value());
}

TEST(LiteralFuzzyMatcherTest, ReturnsScoredCandidateForMultipleEdits) {
  const auto candidate =
      bestCandidate("service", "sxrivxe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_GE(match.distance, 3u);
  EXPECT_GT(match.cost.primaryRankCost, 3u);
  EXPECT_GT(match.rawWeightedCost, 3u);
}

TEST(LiteralFuzzyMatcherTest, SupportsShortWordLiterals) {
  const auto candidate =
      bestCandidate("def", "de", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 2u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 1u);
  EXPECT_EQ(match.cost.budgetCost, 1u);
  EXPECT_EQ(match.cost.primaryRankCost, 2u);
  EXPECT_EQ(match.rawWeightedCost, 1u);
}

TEST(LiteralFuzzyMatcherTest, ShorterLiteralsCarryHigherNormalizedEditCost) {
  const auto shortCandidate =
      bestCandidate("name", "nane", true);
  const auto longCandidate =
      bestCandidate("service", "servixe", true);
  const auto &shortMatch = requireCandidate(shortCandidate);
  const auto &longMatch = requireCandidate(longCandidate);

  EXPECT_EQ(shortMatch.substitutionCount, 1u);
  EXPECT_EQ(longMatch.substitutionCount, 1u);
  EXPECT_GT(shortMatch.cost.primaryRankCost, longMatch.cost.primaryRankCost);
  EXPECT_EQ(shortMatch.cost.primaryRankCost, 3u);
  EXPECT_EQ(longMatch.cost.primaryRankCost, 2u);
  EXPECT_EQ(shortMatch.rawWeightedCost, 2u);
  EXPECT_EQ(longMatch.rawWeightedCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, ProducesScoredCandidateForMissingSuffixKeyword) {
  const auto candidate =
      bestCandidate("module", "Mod", false);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 3u);
  EXPECT_EQ(match.distance, 3u);
  EXPECT_EQ(match.insertionCount, 3u);
  EXPECT_EQ(match.deletionCount, 0u);
}

TEST(LiteralFuzzyMatcherTest, ChoosesBestPrefixInsteadOfEntireInputWindow) {
  const auto candidate =
      bestCandidate("=>", "> RedLight", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 1u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
}

TEST(LiteralFuzzyMatcherTest, ExposesMultipleConsumedCandidatesForOneWindow) {
  const auto candidates = allCandidates("=>", "> RedLight", true);

  ASSERT_FALSE(candidates.empty());
  EXPECT_NE(findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
              return candidate.consumed == 1u && candidate.insertionCount == 1u &&
                     candidate.substitutionCount == 0u;
            }),
            nullptr);
  EXPECT_NE(findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
              return candidate.consumed > 1u;
            }),
            nullptr);
}

TEST(LiteralFuzzyMatcherTest, SupportsDistanceTwoForMediumKeywords) {
  const auto candidate =
      bestCandidate("catalogue", "catalogxoe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.distance, 2u);
  EXPECT_EQ(match.consumed, 10u);
}

TEST(LiteralFuzzyMatcherTest, SupportsDistanceThreeForLongKeywords) {
  const auto candidate = bestCandidate(
      "implementation", "implxmentatoin", true);

  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->distance, 2u);
}

TEST(LiteralFuzzyMatcherTest, ChoosesLowestNormalizedScoreCandidate) {
  const auto candidate =
      bestCandidate("catalogue", "catalogoe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.substitutionCount, 1u);
}

TEST(LiteralFuzzyMatcherTest, ReturnsNoCandidateOnEmptyInput) {
  const auto candidate =
      bestCandidate("module", "", true);

  EXPECT_FALSE(candidate.has_value());
}

TEST(LiteralFuzzyMatcherTest, KeepsAlternativeWithoutSubstitution) {
  const auto candidates = allCandidates("module", "modole", true);

  ASSERT_FALSE(candidates.empty());
  const auto *substitutionCandidate =
      findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
        return candidate.consumed == 6u && candidate.substitutionCount == 1u &&
               candidate.distance == 1u;
      });
  const auto *noSubstitutionCandidate =
      findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
        return candidate.consumed == 6u && candidate.substitutionCount == 0u &&
               candidate.insertionCount == 1u && candidate.deletionCount == 1u;
      });

  ASSERT_NE(substitutionCandidate, nullptr);
  ASSERT_NE(noSubstitutionCandidate, nullptr);
  EXPECT_LT(substitutionCandidate->cost.primaryRankCost,
            noSubstitutionCandidate->cost.primaryRankCost);
  EXPECT_LT(substitutionCandidate->rawWeightedCost,
            noSubstitutionCandidate->rawWeightedCost);
}

TEST(LiteralFuzzyMatcherTest,
     KeepsAlternativeWithoutSubstitutionForSymbolicLiteral) {
  const auto candidates = allCandidates("::==", ":===", true);

  ASSERT_FALSE(candidates.empty());
  EXPECT_NE(findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
              return candidate.consumed == 4u && candidate.substitutionCount == 1u &&
                     candidate.distance == 1u;
            }),
            nullptr);
  EXPECT_NE(findCandidate(candidates, [](const LiteralFuzzyCandidate &candidate) {
              return candidate.consumed == 4u && candidate.substitutionCount == 0u &&
                     candidate.insertionCount == 1u && candidate.deletionCount == 1u;
            }),
            nullptr);
}

TEST(LiteralFuzzyMatcherTest, FrontierIsBoundedByTwoLanesPerConsumedPrefix) {
  const std::string input = "implementationMismatchTail";
  const auto candidates =
      allCandidates("implementation", input, true);

  EXPECT_LE(candidates.size(), input.size() * 2u);
}

// --- Non-ASCII (codepoint-granular) fuzzy matching -------------------------
//
// The Levenshtein DP is indexed by codepoint, so a single multibyte typo costs
// one edit (not one-per-byte). These cases all score distance 2 under a byte-
// granular DP (the prior behaviour) and would therefore be rejected by every
// hard `distance == 1` admission gate; under the codepoint DP they are a single
// edit and recover. `consumed` is still reported as a BYTE offset so callers
// can advance the cursor unchanged.
TEST(LiteralFuzzyMatcherTest, NonAsciiSingleCodepointEditScoresOneCodepoint) {
  struct Case {
    const char *name;
    std::string_view literal;
    std::string_view input;
    std::uint32_t consumedBytes;       // BYTE offset contract for callers
    std::uint32_t consumedCodepoints;  // codepoint count
    std::uint32_t insertionCount;
    std::uint32_t deletionCount;
    std::uint32_t substitutionCount;
  };
  static const Case kCases[] = {
      // "café" (4 cp / 5 bytes) mistyped "cafe": one substitution é -> e.
      // Byte DP: distance 2 (sub of 0xC3 + delete of 0xA9). Codepoint DP: 1.
      {"café -> cafe (accent substitution)", "caf\xC3\xA9", "cafe", 4u, 4u, 0u,
       0u, 1u},
      // "modèle" (6 cp / 7 bytes) mistyped "modele".
      {"modèle -> modele (mid accent)", "mod\xC3\xA8le", "modele", 6u, 6u, 0u,
       0u, 1u},
      // CJK two-codepoint keyword 関数 mistyped 類数. 関 (E9 96 A2) vs 類
      // (E9 A1 9E) differ in two bytes, so the byte DP scores distance 2; the
      // codepoint DP scores a single substitution.
      {"関数 -> 類数 (1 codepoint, 2 byte diff)", "\xE9\x96\xA2\xE6\x95\xB0",
       "\xE9\xA1\x9E\xE6\x95\xB0", 6u, 2u, 0u, 0u, 1u},
  };
  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    const auto candidate =
        bestCandidate(c.literal, c.input, true);
    ASSERT_TRUE(candidate.has_value());
    EXPECT_EQ(candidate->distance, 1u);
    EXPECT_EQ(candidate->operationCount, 1u);
    // `consumed` is a byte offset; `consumedCodepoints` is the codepoint count.
    EXPECT_EQ(candidate->consumed, c.consumedBytes);
    EXPECT_EQ(candidate->consumedCodepoints, c.consumedCodepoints);
    EXPECT_EQ(candidate->insertionCount, c.insertionCount);
    EXPECT_EQ(candidate->deletionCount, c.deletionCount);
    EXPECT_EQ(candidate->substitutionCount, c.substitutionCount);
  }
}

TEST(LiteralFuzzyMatcherTest,
     NonAsciiSingleCodepointTypoPassesHardSingleEditGate) {
  // The entry-probe gate (`distance == 1 && operationCount == 1`) is the
  // tightest fuzzy admission gate. A non-ASCII single-codepoint typo whose
  // bytes differ in two positions is distance 2 under a byte DP — rejected by
  // this gate — and distance 1 under the codepoint DP — admitted. This is the
  // gate-level capability flip the codepoint DP unlocks.
  const auto candidate = bestCandidate(
      "\xE9\x96\xA2\xE6\x95\xB0", "\xE9\xA1\x9E\xE6\x95\xB0", true);
  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->distance, 1u);
  EXPECT_TRUE(allows_entry_probe_fuzzy_candidate(*candidate));
}

TEST(LiteralFuzzyMatcherTest, AsciiCodepointDpRemainsByteIdenticalForConsumed) {
  // Regression anchor: for ASCII input, `consumed` and `consumedCodepoints`
  // are equal (one byte per codepoint), and the distances match the byte DP.
  const auto candidate =
      bestCandidate("service", "servixe", true);
  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->distance, 1u);
  EXPECT_EQ(candidate->consumed, 7u);
  EXPECT_EQ(candidate->consumedCodepoints, 7u);
  EXPECT_EQ(candidate->consumed, candidate->consumedCodepoints);
}
