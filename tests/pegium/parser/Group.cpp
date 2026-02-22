#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(GroupTest, ParseTerminalConsumesElementsInSequence) {
  auto group = ":"_kw + ";"_kw;
  std::string_view input = ":;x";

  auto result = group.parse_terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(GroupTest, ParseRuleRollsBackOnFailure) {
  auto group = ":"_kw + ";"_kw;
  auto context = ContextBuilder().build();

  pegium::CstBuilder koBuilder("::");
  ParseState koState{koBuilder, context};
  auto ko = group.parse_rule(koState);
  EXPECT_FALSE(ko);
  auto koRoot = koBuilder.finalize();
  EXPECT_EQ(koRoot->begin(), koRoot->end());

  pegium::CstBuilder okBuilder(":;");
  ParseState okState{okBuilder, context};
  auto ok = group.parse_rule(okState);
  EXPECT_TRUE(ok);
  auto okRoot = okBuilder.finalize();
  EXPECT_NE(okRoot->begin(), okRoot->end());
}

TEST(GroupTest, ParseTerminalReportsFailureOffsetOfFailingElement) {
  auto group = "ab"_kw + "cd"_kw;
  std::string_view input = "abX";

  auto result = group.parse_terminal(input);
  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(GroupTest, OperatorPlusCompositionsRemainFlattenedAndPrintable) {
  auto leftAssoc = ("a"_kw + "b"_kw) + "c"_kw;
  auto rightAssoc = "a"_kw + ("b"_kw + "c"_kw);
  auto extended = (("a"_kw + "b"_kw) + "c"_kw) + "d"_kw;

  {
    std::string_view input = "abcX";
    auto result = leftAssoc.parse_terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 3);
  }
  {
    std::string_view input = "abcX";
    auto result = rightAssoc.parse_terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 3);
  }
  {
    std::string_view input = "abcdX";
    auto result = extended.parse_terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 4);
  }

  std::ostringstream leftText;
  leftText << leftAssoc;
  EXPECT_EQ(leftText.str(), "('a' 'b' 'c')");

  std::ostringstream mergedText;
  mergedText << extended;
  EXPECT_EQ(mergedText.str(), "('a' 'b' 'c' 'd')");
}

TEST(GroupTest, ParseTerminalPointerOverloadWorks) {
  auto group = ":"_kw + ";"_kw;
  std::string_view input = ":;x";

  auto result = group.parse_terminal(input.begin(), input.end());
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(GroupTest, ExplicitOperatorPlusOverloadsPreserveOrder) {
  auto middleGroup = "b"_kw + "c"_kw;
  auto prefixed = "a"_kw + std::move(middleGroup);
  std::string_view prefixedInput = "abc!";
  auto prefixedResult = prefixed.parse_terminal(prefixedInput);
  EXPECT_TRUE(prefixedResult.IsValid());
  EXPECT_EQ(prefixedResult.offset - prefixedInput.begin(), 3);

  auto startGroup = "a"_kw + "b"_kw;
  auto suffixed = std::move(startGroup) + "c"_kw;
  std::string_view suffixedInput = "abc!";
  auto suffixedResult = suffixed.parse_terminal(suffixedInput);
  EXPECT_TRUE(suffixedResult.IsValid());
  EXPECT_EQ(suffixedResult.offset - suffixedInput.begin(), 3);
}

TEST(GroupTest, ExposesGrammarKind) {
  auto group = ":"_kw + ";"_kw;
  EXPECT_EQ(group.getKind(), pegium::grammar::ElementKind::Group);
}
