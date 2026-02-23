#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(CharacterRangeTest, MatchesConfiguredRange) {
  auto range = "a-c"_cr;
  std::string_view ok = "b";
  std::string_view ko = "z";

  auto okResult = range.terminal(ok);
  EXPECT_TRUE(okResult.IsValid());
  EXPECT_EQ(okResult.offset - ok.begin(), 1);

  auto koResult = range.terminal(ko);
  EXPECT_FALSE(koResult.IsValid());
}

TEST(CharacterRangeTest, CaseInsensitiveRangeMatchesUpperCase) {
  auto range = "a-z"_cr.i();
  std::string_view input = "M";

  auto result = range.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 1);
}

TEST(CharacterRangeTest, ParseRuleAddsCstNodeOnSuccess) {
  auto range = "0-9"_cr;
  pegium::CstBuilder builder("7");
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = range.rule(ctx);
  EXPECT_TRUE(result);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), "7");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(CharacterRangeTest, ParseTerminalFailsOnEmptyInputAndParseRuleRewinds) {
  auto range = "a-c"_cr;

  std::string_view empty;
  auto terminal = range.terminal(empty.begin(), empty.end());
  EXPECT_FALSE(terminal.IsValid());
  EXPECT_EQ(terminal.offset, empty.begin());

  pegium::CstBuilder builder("x");
  auto skipper = SkipperBuilder().build();
  ParseContext ctx{builder, skipper};
  auto rule = range.rule(ctx);
  EXPECT_FALSE(rule);
  EXPECT_EQ(ctx.cursor(), builder.getText().begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(CharacterRangeTest, LvalueInsensitiveRangeReturnsNewObject) {
  auto original = "A-C"_cr;
  auto insensitive = original.i();

  std::string_view lower = "b";
  auto originalLower = original.terminal(lower);
  EXPECT_FALSE(originalLower.IsValid());

  auto insensitiveLower = insensitive.terminal(lower);
  EXPECT_TRUE(insensitiveLower.IsValid());
  EXPECT_EQ(insensitiveLower.offset - lower.begin(), 1);

  std::string_view upper = "B";
  auto originalUpper = original.terminal(upper);
  EXPECT_TRUE(originalUpper.IsValid());
  EXPECT_EQ(originalUpper.offset - upper.begin(), 1);

  auto insensitiveUpper = insensitive.terminal(upper);
  EXPECT_TRUE(insensitiveUpper.IsValid());
  EXPECT_EQ(insensitiveUpper.offset - upper.begin(), 1);
}

TEST(CharacterRangeTest, PrintCollapsesSinglePairAndRangeSequences) {
  std::ostringstream single;
  single << "x"_cr;
  EXPECT_EQ(single.str(), "[x]");

  std::ostringstream pair;
  pair << "xy"_cr;
  EXPECT_EQ(pair.str(), "[xy]");

  std::ostringstream span;
  span << "a-c"_cr;
  EXPECT_EQ(span.str(), "[a-c]");
}
