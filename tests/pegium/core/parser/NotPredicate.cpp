#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(NotPredicateTest, SucceedsWhenSubExpressionDoesNotMatch) {
  auto predicate = !"a"_kw;
  std::string input = "bc";

  auto result = predicate.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(NotPredicateTest, FailsWhenSubExpressionMatches) {
  auto predicate = !"a"_kw;
  std::string input = "abc";

  auto result = predicate.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(NotPredicateTest, ParseRuleDoesNotAddNodes) {
  auto predicate = !"a"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("bc");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(predicate, ctx);
  EXPECT_TRUE(result);

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, ParseRuleFailsWhenSubExpressionMatchesAndRewinds) {
  auto predicate = !"a"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("a:");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(predicate, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, StrictSafeProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto predicate = !"a"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("bc");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(predicate, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("a:");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(predicate, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}
