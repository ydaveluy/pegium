#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

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

TEST(LiteralTest, CaseInsensitiveLiteralFailsOnMismatchAndPrintsSuffix) {
  auto literal = "abc"_kw.i();
  std::string input = "AbX";

  auto result = literal.terminal(input);
  EXPECT_EQ(result, nullptr);

  std::ostringstream os;
  os << literal;
  EXPECT_EQ(os.str(), "'abc'i");
}

TEST(LiteralTest, NonAlphabeticInsensitiveLiteralRemainsCaseSensitive) {
  auto literal = "123"_kw.i();
  std::string input = "123";

  auto result = literal.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 3);

  std::ostringstream os;
  os << literal;
  EXPECT_EQ(os.str(), "'123'");
}

TEST(LiteralTest, PrintEscapesControlAndQuoteCharacters) {
  auto literal = "\n\t\\'"_kw;
  std::ostringstream os;
  os << literal;

  EXPECT_EQ(os.str(), "'\\n\\t\\\\\\''");
}

TEST(LiteralTest, PrintEscapesAdditionalControlCharacters) {
  auto literal = "\r\v\f\b\a\""_kw;
  std::ostringstream os;
  os << literal;

  EXPECT_EQ(os.str(), "'\\r\\v\\f\\b\\a\\\"'");
}

TEST(LiteralTest, PrintEscapesUnknownNonPrintableCharactersAsHex) {
  auto literal = "\x01"_kw;
  std::ostringstream os;
  os << literal;

  EXPECT_EQ(os.str(), "'\\x01'");
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
