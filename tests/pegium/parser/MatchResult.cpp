#include <gtest/gtest.h>
#include <pegium/parser/ParseExpression.hpp>

using namespace pegium::parser;

TEST(MatchResultTest, FactoriesProduceExpectedStates) {
  const char *base = "abcdef";
  auto ok = MatchResult::success(base + 2);
  auto ko = MatchResult::failure(base + 4);

  EXPECT_TRUE(ok.IsValid());
  EXPECT_FALSE(ko.IsValid());
  EXPECT_EQ(ok.offset, base + 2);
  EXPECT_EQ(ko.offset, base + 4);
}
