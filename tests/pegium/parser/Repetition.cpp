#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <limits>

using namespace pegium::parser;

TEST(RepetitionTest, OptionSomeAndManyBehaveAsExpected) {
  {
    std::string input = "";
    auto result = option("a"_kw).terminal(input);
    EXPECT_EQ(result, (input).c_str());
  }

  {
    std::string input = "";
    auto result = some("a"_kw).terminal(input);
    EXPECT_EQ(result, nullptr);
  }

  {
    std::string input = "aaab";
    auto result = many("a"_kw).terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 3);
  }
}

TEST(RepetitionTest, ParseRuleHandlesOptionalStarAndPlusVariants) {
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("b");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(option("a"_kw), ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("aaab");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(many(":"_kw), ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 0);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("aaab");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(some(":"_kw), ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 0);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(":::b");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(many(":"_kw), ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(":::b");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(some(":"_kw), ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("bbb");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(some(":"_kw), ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}

TEST(RepetitionTest, StarRewindsSkippedHiddenNodesWhenNextOccurrenceFails) {
  TerminalRule<> ws{"WS", some(s)};
  auto skipper = SkipperBuilder().hide(ws).build();
  auto expression = many("a"_kw);

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   !");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  ParseContext ctx{builder, skipper};

  const bool result = parse(expression, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_FALSE((*it).isHidden());
  EXPECT_EQ((*it).getText(), "a");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(RepetitionTest, WithLocalSkipperCanMatchSeparatedRepetitions) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto expression = some("a"_kw).skip(ignored(ws));

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   a");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  EXPECT_TRUE(parse(expression, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);
}

TEST(RepetitionTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto expression = some("a"_kw).skip(ignored(ws));
  auto trailing = "b"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   a   b");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  ASSERT_TRUE(parse(expression, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);

  const auto beforeSkip = ctx.cursorOffset();
  ctx.skip();
  EXPECT_EQ(ctx.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailing, ctx));
}

TEST(RepetitionTest, FixedAndBoundedRepetitionsHandleLimits) {
  {
    std::string input = "aaab";
    auto result = repeat<2>("a"_kw).terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 2);
  }

  {
    std::string input = "ab";
    auto result = repeat<2>("a"_kw).terminal(input);
    EXPECT_EQ(result, nullptr);
  }

  {
    std::string input = "aaab";
    auto result = repeat<1, 3>("a"_kw).terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 3);
  }

  {
    std::string input = "a,a,a!";
    auto result = some("a"_kw, ","_kw).terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 5);
  }
}

TEST(RepetitionTest, BoundedParseRuleRespectsMinAndMax) {
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness(":::x");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(repeat<1, 3>(":"_kw), ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("x");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = parse(repeat<1, 3>(":"_kw), ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}

TEST(RepetitionTest, FixedParseRuleCanFailAfterPartialConsumption) {
  auto skipper = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness(":x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  ParseContext ctx{builder, skipper};
  auto result = parse(repeat<2>(":"_kw), ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);
}
