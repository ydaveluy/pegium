#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
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

TEST(CharacterRangeTest, StrictProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto range = "a-c"_cr;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("b");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(range, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("z");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(range, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}
