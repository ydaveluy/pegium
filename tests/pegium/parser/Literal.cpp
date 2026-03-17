#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
using namespace pegium::parser;

TEST(LiteralTest, ParseRuleHandlesWordBoundaryAndEndOfInput) {
  auto literal = "abc"_kw;
  auto context = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext state{builder, context};
    auto result = parse(literal, state);
    EXPECT_TRUE(result);
    EXPECT_EQ(state.cursor() - input.begin(), 3);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("abc:");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext state{builder, context};
    auto result = parse(literal, state);
    EXPECT_TRUE(result);
    EXPECT_EQ(state.cursor() - input.begin(), 3);
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("abcx");
    auto &builder = builderHarness.builder;
    ParseContext state{builder, context};
    auto result = parse(literal, state);
    EXPECT_FALSE(result);
  }
}

TEST(LiteralTest, CaseInsensitiveLiteralMatchesMixedCaseInput) {
  auto literal = "abc"_kw.i();
  std::string input = "AbC";

  auto result = literal.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 3);
}

TEST(LiteralTest, ParseTerminalFailsOnMismatch) {
  auto literal = "abc"_kw;
  std::string input = "abX";

  auto result = literal.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(LiteralTest, NonAlphabeticInsensitiveLiteralRemainsCaseSensitive) {
  auto literal = "123"_kw.i();
  std::string input = "123";

  auto result = literal.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 3);
}

TEST(LiteralTest, ParseTerminalFailsWhenInputIsTooShort) {
  auto literal = "abcd"_kw;
  std::string input = "abc";

  auto result = literal.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(LiteralTest, WordBoundaryFailureKeepsCursorAndTreeUntouched) {
  auto literal = "abc"_kw;
  auto context = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("abcx");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  ParseContext state{builder, context};
  auto result = parse(literal, state);
  EXPECT_FALSE(result);
  EXPECT_EQ(state.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(LiteralTest, StrictProbeDoesNotChangeCursorTreeOrMaxCursor) {
  auto literal = "abc"_kw;
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("abc:");
    auto &builder = builderHarness.builder;
    ParseContext state{builder, skipper};
    const auto *cursor = state.cursor();
    const auto *maxCursor = state.maxCursor();

    EXPECT_TRUE(probe(literal, state));
    EXPECT_EQ(state.cursor(), cursor);
    EXPECT_EQ(state.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("abX");
    auto &builder = builderHarness.builder;
    ParseContext state{builder, skipper};
    const auto *cursor = state.cursor();
    const auto *maxCursor = state.maxCursor();

    EXPECT_FALSE(probe(literal, state));
    EXPECT_EQ(state.cursor(), cursor);
    EXPECT_EQ(state.maxCursor(), maxCursor);
    EXPECT_EQ(builder.getRootCstNode()->begin(), builder.getRootCstNode()->end());
  }
}
