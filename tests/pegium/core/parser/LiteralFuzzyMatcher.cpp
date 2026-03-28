#include <algorithm>
#include <gtest/gtest.h>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <string>

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

} // namespace

TEST(LiteralFuzzyMatcherTest, SupportsSingleDeletionFromInput) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modle", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 5u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 1u);
  EXPECT_EQ(match.cost.budgetCost, 1u);
  EXPECT_EQ(match.cost.primaryRankCost, 1u);
  EXPECT_EQ(match.cost.secondaryRankCost, 1u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleInsertionInInput) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modulee", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 7u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.deletionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 4u);
  EXPECT_EQ(match.cost.budgetCost, 4u);
  EXPECT_EQ(match.cost.primaryRankCost, 4u);
  EXPECT_EQ(match.cost.secondaryRankCost, 4u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleAdjacentTransposition) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modlue", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 6u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.transpositionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.cost.budgetCost, 2u);
  EXPECT_EQ(match.cost.primaryRankCost, 2u);
  EXPECT_EQ(match.cost.secondaryRankCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleSubstitution) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("service", "servixe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 7u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.substitutionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.cost.budgetCost, 2u);
  EXPECT_EQ(match.cost.primaryRankCost, 2u);
  EXPECT_EQ(match.cost.secondaryRankCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, ExactCaseInsensitiveMatchDoesNotRequireFuzzyEdit) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "ModUle", false);

  EXPECT_FALSE(candidate.has_value());
}

TEST(LiteralFuzzyMatcherTest, ReturnsScoredCandidateForMultipleEdits) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("service", "sxrivxe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_GE(match.distance, 3u);
  EXPECT_GT(match.cost.primaryRankCost, 3u);
  EXPECT_GT(match.cost.secondaryRankCost, 3u);
}

TEST(LiteralFuzzyMatcherTest, SupportsShortWordLiterals) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("def", "de", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 2u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 1u);
  EXPECT_EQ(match.cost.budgetCost, 1u);
  EXPECT_EQ(match.cost.primaryRankCost, 2u);
  EXPECT_EQ(match.cost.secondaryRankCost, 1u);
}

TEST(LiteralFuzzyMatcherTest, ShorterLiteralsCarryHigherNormalizedEditCost) {
  const auto shortCandidate =
      find_best_literal_fuzzy_candidate("name", "nane", true);
  const auto longCandidate =
      find_best_literal_fuzzy_candidate("service", "servixe", true);
  const auto &shortMatch = requireCandidate(shortCandidate);
  const auto &longMatch = requireCandidate(longCandidate);

  EXPECT_EQ(shortMatch.substitutionCount, 1u);
  EXPECT_EQ(longMatch.substitutionCount, 1u);
  EXPECT_GT(shortMatch.cost.primaryRankCost, longMatch.cost.primaryRankCost);
  EXPECT_EQ(shortMatch.cost.primaryRankCost, 3u);
  EXPECT_EQ(longMatch.cost.primaryRankCost, 2u);
  EXPECT_EQ(shortMatch.cost.secondaryRankCost, 2u);
  EXPECT_EQ(longMatch.cost.secondaryRankCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, ProducesScoredCandidateForMissingSuffixKeyword) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "Mod", false);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 3u);
  EXPECT_EQ(match.distance, 3u);
  EXPECT_EQ(match.insertionCount, 3u);
  EXPECT_EQ(match.deletionCount, 0u);
}

TEST(LiteralFuzzyMatcherTest, ChoosesBestPrefixInsteadOfEntireInputWindow) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("=>", "> RedLight", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 1u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
}

TEST(LiteralFuzzyMatcherTest, ExposesMultipleConsumedCandidatesForOneWindow) {
  const auto candidates = find_literal_fuzzy_candidates("=>", "> RedLight", true);

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
      find_best_literal_fuzzy_candidate("catalogue", "catalogxoe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.distance, 2u);
  EXPECT_EQ(match.consumed, 10u);
}

TEST(LiteralFuzzyMatcherTest, SupportsDistanceThreeForLongKeywords) {
  const auto candidate = find_best_literal_fuzzy_candidate(
      "implementation", "implxmentatoin", true);

  ASSERT_TRUE(candidate.has_value());
  EXPECT_EQ(candidate->distance, 2u);
}

TEST(LiteralFuzzyMatcherTest, ChoosesLowestNormalizedScoreCandidate) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("catalogue", "catalogoe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.substitutionCount, 1u);
}

TEST(LiteralFuzzyMatcherTest, ReturnsNoCandidateOnEmptyInput) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "", true);

  EXPECT_FALSE(candidate.has_value());
}

TEST(LiteralFuzzyMatcherTest, KeepsAlternativeWithoutSubstitution) {
  const auto candidates = find_literal_fuzzy_candidates("module", "modole", true);

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
  EXPECT_LT(substitutionCandidate->cost.secondaryRankCost,
            noSubstitutionCandidate->cost.secondaryRankCost);
}

TEST(LiteralFuzzyMatcherTest,
     KeepsAlternativeWithoutSubstitutionForSymbolicLiteral) {
  const auto candidates = find_literal_fuzzy_candidates("::==", ":===", true);

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
      find_literal_fuzzy_candidates("implementation", input, true);

  EXPECT_LE(candidates.size(), input.size() * 2u);
}
