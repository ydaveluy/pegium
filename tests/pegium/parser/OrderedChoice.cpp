#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(OrderedChoiceTest, ChoosesFirstMatchingAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  std::string input = "abx";

  auto result = choice.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 2);
}

TEST(OrderedChoiceTest, FallsBackToNextAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  std::string input = "ax";

  auto result = choice.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 1);
}

TEST(OrderedChoiceTest, ParseRuleAddsNodesFromSelectedAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("a");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(choice, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(OrderedChoiceTest, ParseTerminalFailsWhenNoAlternativeMatches) {
  auto choice = "ab"_kw | "cd"_kw;
  std::string input = "xy";

  auto result = choice.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(OrderedChoiceTest, ParseRuleRewindsBeforeTryingNextAlternative) {
  auto choice = (":"_kw + ";"_kw) | ":"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness(":x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(choice, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), ":");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(OrderedChoiceTest, WithLocalSkipperCanMatchAlternativeContainingGroup) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto choice = (("a"_kw + "b"_kw) | "c"_kw).skip(ignored(ws));

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  EXPECT_TRUE(parse(choice, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);
}

TEST(OrderedChoiceTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto choice = (("a"_kw + "b"_kw) | "c"_kw).skip(ignored(ws));
  auto trailing = "c"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b   c");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  ASSERT_TRUE(parse(choice, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);

  const auto beforeSkip = ctx.cursorOffset();
  ctx.skip();
  EXPECT_EQ(ctx.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailing, ctx));
}

TEST(OrderedChoiceTest, ParseRuleFailureRewindsCursorAndLeavesNoNodes) {
  auto choice = ("a"_kw + "b"_kw) | ("c"_kw + "d"_kw);
  auto builderHarness = pegium::test::makeCstBuilderHarness("ax");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(choice, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(OrderedChoiceTest, StrictSafeProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto choice = "ab"_kw | "a"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("ab:");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_TRUE(probe(choice, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xy");
    auto &builder = builderHarness.builder;
    ParseContext ctx{builder, skipper};
    const auto *cursor = ctx.cursor();
    const auto *maxCursor = ctx.maxCursor();

    EXPECT_FALSE(probe(choice, ctx));
    EXPECT_EQ(ctx.cursor(), cursor);
    EXPECT_EQ(ctx.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}

TEST(OrderedChoiceTest, FastProbeMatchesStrictParseOutcome) {
  auto choice = "ab"_kw | "a"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto fastHarness = pegium::test::makeCstBuilderHarness("ab:");
    auto &fastBuilder = fastHarness.builder;
    ParseContext fastCtx{fastBuilder, skipper};

    auto parseHarness = pegium::test::makeCstBuilderHarness("ab:");
    auto &parseBuilder = parseHarness.builder;
    ParseContext parseCtx{parseBuilder, skipper};

    EXPECT_EQ(attempt_fast_probe(fastCtx, choice), parse(choice, parseCtx));
  }

  {
    auto fastHarness = pegium::test::makeCstBuilderHarness("xy");
    auto &fastBuilder = fastHarness.builder;
    ParseContext fastCtx{fastBuilder, skipper};

    auto parseHarness = pegium::test::makeCstBuilderHarness("xy");
    auto &parseBuilder = parseHarness.builder;
    ParseContext parseCtx{parseBuilder, skipper};

    EXPECT_EQ(attempt_fast_probe(fastCtx, choice), parse(choice, parseCtx));
  }
}
