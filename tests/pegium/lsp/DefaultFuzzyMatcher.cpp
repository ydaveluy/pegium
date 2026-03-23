#include <gtest/gtest.h>

#include <pegium/lsp/support/DefaultFuzzyMatcher.hpp>

namespace pegium {
namespace {

TEST(DefaultFuzzyMatcherTest, MatchesFullStringAndPrefixesIgnoringCase) {
  DefaultFuzzyMatcher matcher;

  EXPECT_TRUE(matcher.match("grammar", "Grammar"));
  EXPECT_TRUE(matcher.match("gr", "Grammar"));
  EXPECT_TRUE(matcher.match("GK", "GrammarKeyword"));
}

TEST(DefaultFuzzyMatcherTest, MatchesSubsequencesAndRejectsMissingPatterns) {
  DefaultFuzzyMatcher matcher;

  EXPECT_TRUE(matcher.match("gmr", "Grammar"));
  EXPECT_TRUE(matcher.match("dsp", "documentSymbolProvider"));
  EXPECT_FALSE(matcher.match("ram", "Grammar"));
  EXPECT_FALSE(matcher.match("xyz", "Grammar"));
}

} // namespace
} // namespace pegium
