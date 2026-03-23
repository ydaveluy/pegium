#include <algorithm>
#include <gtest/gtest.h>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>

using namespace pegium::parser::detail;

namespace {

const LiteralFuzzyCandidate &
requireCandidate(const std::optional<LiteralFuzzyCandidate> &candidate) {
  EXPECT_TRUE(candidate.has_value());
  return *candidate;
}

} // namespace

TEST(LiteralFuzzyMatcherTest, SupportsSingleDeletionFromInput) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modle", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 5u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.normalizedEditCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleInsertionInInput) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modulee", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 7u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.deletionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.normalizedEditCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleAdjacentTransposition) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("module", "modlue", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 6u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.transpositionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.normalizedEditCost, 2u);
}

TEST(LiteralFuzzyMatcherTest, SupportsSingleSubstitution) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("service", "servixe", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 7u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.substitutionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 3u);
  EXPECT_EQ(match.normalizedEditCost, 3u);
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
  EXPECT_GT(match.normalizedEditCost, 3u);
}

TEST(LiteralFuzzyMatcherTest, SupportsShortWordLiterals) {
  const auto candidate =
      find_best_literal_fuzzy_candidate("def", "de", true);
  const auto &match = requireCandidate(candidate);

  EXPECT_EQ(match.consumed, 2u);
  EXPECT_EQ(match.distance, 1u);
  EXPECT_EQ(match.insertionCount, 1u);
  EXPECT_EQ(match.rawWeightedCost, 2u);
  EXPECT_EQ(match.normalizedEditCost, 4u);
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
  EXPECT_GT(shortMatch.normalizedEditCost, longMatch.normalizedEditCost);
  EXPECT_EQ(shortMatch.normalizedEditCost, 4u);
  EXPECT_EQ(longMatch.normalizedEditCost, 3u);
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
