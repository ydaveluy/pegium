#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(AndPredicateTest, LookaheadSucceedsWithoutConsumingInput) {
  auto lookahead = &":"_kw;
  std::string input = ":abc";

  auto result = lookahead.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(AndPredicateTest, LookaheadFailsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  std::string input = "xabc";

  auto result = lookahead.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(AndPredicateTest, ParseRuleDoesNotModifyParentNode) {
  auto lookahead = &":"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness(":abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(lookahead, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, ParseRuleFailsAndRewindsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("xabc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(lookahead, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, StrictSafeProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto lookahead = &":"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(":abc");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(lookahead, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xabc");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(lookahead, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}
