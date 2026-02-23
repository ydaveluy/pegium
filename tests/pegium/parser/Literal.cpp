#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

/*TEST(LiteralTest, ParseRuleHandlesWordBoundaryAndEndOfInput) {
  auto literal = "abc"_kw;
  auto context = SkipperBuilder().build();

  {
    pegium::CstBuilder builder("abc");
    const auto input = builder.getText();
    ParseContext state{builder, context};
    auto result = literal.rule(state);
    EXPECT_TRUE(result);
    EXPECT_EQ(state.cursor() - input.begin(), 3);
  }

  {
    pegium::CstBuilder builder("abc:");
    const auto input = builder.getText();
    ParseContext state{builder, context};
    auto result = literal.rule(state);
    EXPECT_TRUE(result);
    EXPECT_EQ(state.cursor() - input.begin(), 3);
  }

  {
    pegium::CstBuilder builder("abcx");
    ParseContext state{builder, context};
    auto result = literal.rule(state);
    EXPECT_FALSE(result);
  }
}*/

TEST(LiteralTest, CaseInsensitiveLiteralMatchesMixedCaseInput) {
  auto literal = "abc"_kw.i();
  std::string_view input = "AbC";

  auto result = literal.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 3);
}

TEST(LiteralTest, ParseTerminalReportsMismatchOffset) {
  auto literal = "abc"_kw;
  std::string_view input = "abX";

  auto result = literal.terminal(input);
  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(LiteralTest, CaseInsensitiveLiteralReportsOffsetAndPrintsSuffix) {
  auto literal = "abc"_kw.i();
  std::string_view input = "AbX";

  auto result = literal.terminal(input);
  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);

  std::ostringstream os;
  os << literal;
  EXPECT_EQ(os.str(), "'abc'i");
}

TEST(LiteralTest, NonAlphabeticInsensitiveLiteralRemainsCaseSensitive) {
  auto literal = "123"_kw.i();
  std::string_view input = "123";

  auto result = literal.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 3);

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
  std::string_view input = "abc";

  auto result = literal.terminal(input.begin(), input.end());
  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

/*TEST(LiteralTest, WordBoundaryFailureKeepsCursorAndTreeUntouched) {
  auto literal = "abc"_kw;
  auto context = SkipperBuilder().build();
  pegium::CstBuilder builder("abcx");
  const auto input = builder.getText();

  ParseContext state{builder, context};
  auto result = literal.rule(state);
  EXPECT_FALSE(result);
  EXPECT_EQ(state.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}*/