#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(CharacterRangeTest, MatchesConfiguredRange) {
  auto range = "a-c"_cr;
  std::string ok = "b";
  std::string ko = "z";

  auto okResult = range.terminal(ok);
  EXPECT_NE(okResult, nullptr);
  EXPECT_EQ(okResult - ok.c_str(), 1);

  auto koResult = range.terminal(ko);
  EXPECT_EQ(koResult, nullptr);
}


TEST(CharacterRangeTest, ParseRuleAddsCstNodeOnSuccess) {
  auto range = "0-9"_cr;
  auto builderHarness = pegium::test::makeCstBuilderHarness("7");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(range, ctx);
  EXPECT_TRUE(result);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), "7");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(CharacterRangeTest, ParseTerminalFailsOnEmptyInputAndParseRuleRewinds) {
  auto range = "a-c"_cr;

  std::string empty;
  auto terminal = range.terminal(empty);
  EXPECT_EQ(terminal, nullptr);

  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  ParseContext ctx{builder, skipper};
  auto rule = parse(range, ctx);
  EXPECT_FALSE(rule);
  EXPECT_EQ(ctx.cursor(), builder.getText().begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
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
