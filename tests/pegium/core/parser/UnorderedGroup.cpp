#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(UnorderedGroupTest, ParseTerminalAcceptsAnyOrder) {
  auto group = ":"_kw & ";"_kw;

  {
    std::string input = ":;";
    auto result = group.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 2);
  }

  {
    std::string input = ";:";
    auto result = group.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 2);
  }

  {
    std::string input = ":x";
    auto result = group.terminal(input);
    EXPECT_EQ(result, nullptr);
  }
}

TEST(UnorderedGroupTest, ParseRuleAddsNodesForParsedElements) {
  auto group = ":"_kw & ";"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness(";:");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(group, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 2);

  auto root = builder.getRootCstNode();
  std::size_t count = 0;
  for (const auto &child : *root) {
    (void)child;
    ++count;
  }
  EXPECT_EQ(count, 2u);
}

TEST(UnorderedGroupTest, ParseRuleRequiresDistinctConsumptionPerElement) {
  auto group = "a"_kw & "a"_kw;

  {
    std::string input = "a";
    auto terminal = group.terminal(input);
    EXPECT_EQ(terminal, nullptr);
  }

  auto builderHarness = pegium::test::makeCstBuilderHarness("a");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  const bool rule = parse(group, ctx);

  // This is the expected behavior: each element should consume its own span.
  // The current implementation may still validate the same span twice.
  EXPECT_FALSE(rule);
  EXPECT_EQ(ctx.cursor(), input.begin());
}

TEST(UnorderedGroupTest, WithLocalSkipperCanMatchInternalSeparators) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto group = ("a"_kw & "b"_kw).skip(ignored(ws));

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  EXPECT_TRUE(parse(group, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);
}

TEST(UnorderedGroupTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto defaultSkipper = skip();
  auto group = ("a"_kw & "b"_kw).skip(ignored(ws));
  auto trailing = "c"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b   c");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  ASSERT_TRUE(parse(group, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);

  const auto beforeSkip = ctx.cursorOffset();
  ctx.skip();
  EXPECT_EQ(ctx.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailing, ctx));
}
