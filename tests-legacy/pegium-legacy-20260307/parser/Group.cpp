#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(GroupTest, ParseTerminalConsumesElementsInSequence) {
  auto group = ":"_kw + ";"_kw;
  std::string input = ":;x";

  auto result = group.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - (input).c_str(), 2);
}

TEST(GroupTest, ParseRuleFailureDoesNotRollbackLocally) {
  auto group = ":"_kw + ";"_kw;
  auto context = SkipperBuilder().build();

  auto koBuilderHarness = pegium::test::makeCstBuilderHarness("::");
  auto &koBuilder = koBuilderHarness.builder;
  ParseContext koState{koBuilder, context};
  auto ko = parse(group, koState);
  EXPECT_FALSE(ko);
  auto koRoot = koBuilder.getRootCstNode();
  EXPECT_NE(koRoot->begin(), koRoot->end());

  auto okBuilderHarness = pegium::test::makeCstBuilderHarness(":;");
  auto &okBuilder = okBuilderHarness.builder;
  ParseContext okState{okBuilder, context};
  auto ok = parse(group, okState);
  EXPECT_TRUE(ok);
  auto okRoot = okBuilder.getRootCstNode();
  EXPECT_NE(okRoot->begin(), okRoot->end());
}

TEST(GroupTest, WithLocalSkipperCanMatchInternalSeparators) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto group = ("a"_kw + "b"_kw).with_skipper(localSkipper);

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext state{builder, defaultSkipper};

  EXPECT_TRUE(parse(group, state));
  EXPECT_EQ(state.cursorOffset(), 5u);
}

TEST(GroupTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto group = ("a"_kw + "b"_kw).with_skipper(localSkipper);
  auto trailingLiteral = "c"_kw;

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b   c");
  auto &builder = builderHarness.builder;
  ParseContext state{builder, defaultSkipper};

  ASSERT_TRUE(parse(group, state));
  EXPECT_EQ(state.cursorOffset(), 5u);

  const auto beforeSkip = state.cursorOffset();
  state.skip();
  EXPECT_EQ(state.cursorOffset(), beforeSkip);
  EXPECT_FALSE(parse(trailingLiteral, state));
}

TEST(GroupTest, ParseTerminalFailsWhenAnElementDoesNotMatch) {
  auto group = "ab"_kw + "cd"_kw;
  std::string input = "abX";

  auto result = group.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(GroupTest, OperatorPlusCompositionsRemainFlattenedAndPrintable) {
  auto leftAssoc = ("a"_kw + "b"_kw) + "c"_kw;
  auto rightAssoc = "a"_kw + ("b"_kw + "c"_kw);
  auto extended = (("a"_kw + "b"_kw) + "c"_kw) + "d"_kw;

  {
    std::string input = "abcX";
    auto result = leftAssoc.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 3);
  }
  {
    std::string input = "abcX";
    auto result = rightAssoc.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 3);
  }
  {
    std::string input = "abcdX";
    auto result = extended.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - (input).c_str(), 4);
  }

  std::ostringstream leftText;
  leftText << leftAssoc;
  EXPECT_EQ(leftText.str(), "('a' 'b' 'c')");

  std::ostringstream mergedText;
  mergedText << extended;
  EXPECT_EQ(mergedText.str(), "('a' 'b' 'c' 'd')");
}

TEST(GroupTest, LvaluePlusCompositionsAreFlattened) {
  auto left = "a"_kw + "b"_kw;
  auto right = "c"_kw + "d"_kw;
  const auto constLeft = "a"_kw + "b"_kw;
  const auto constRight = "e"_kw + "f"_kw;

  std::ostringstream lhsLvalue;
  lhsLvalue << (left + "e"_kw);
  EXPECT_EQ(lhsLvalue.str(), "('a' 'b' 'e')");

  std::ostringstream rhsLvalue;
  rhsLvalue << ("e"_kw + right);
  EXPECT_EQ(rhsLvalue.str(), "('e' 'c' 'd')");

  std::ostringstream bothLvalue;
  bothLvalue << (left + right);
  EXPECT_EQ(bothLvalue.str(), "('a' 'b' 'c' 'd')");

  std::ostringstream constLhsRvalueRhs;
  constLhsRvalueRhs << (constLeft + ("e"_kw + "f"_kw));
  EXPECT_EQ(constLhsRvalueRhs.str(), "('a' 'b' 'e' 'f')");

  std::ostringstream rvalueLhsConstRhs;
  rvalueLhsConstRhs << (("a"_kw + "b"_kw) + constRight);
  EXPECT_EQ(rvalueLhsConstRhs.str(), "('a' 'b' 'e' 'f')");
}

TEST(GroupTest, WithSkipperPlusCompositionsStayNestedOnBothSides) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto plain = "c"_kw;
  auto withSkipper = ("a"_kw + "b"_kw).with_skipper(localSkipper);

  std::ostringstream withSkipperLeft;
  withSkipperLeft << (withSkipper + plain);
  EXPECT_EQ(withSkipperLeft.str(), "(('a' 'b') 'c')");

  std::ostringstream withSkipperRight;
  withSkipperRight << (plain + withSkipper);
  EXPECT_EQ(withSkipperRight.str(), "('c' ('a' 'b'))");
}

TEST(GroupTest, ParseTerminalPointerOverloadWorks) {
  auto group = ":"_kw + ";"_kw;
  std::string input = ":;x";

  auto result = group.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 2);
}

TEST(GroupTest, ExplicitOperatorPlusOverloadsPreserveOrder) {
  auto middleGroup = "b"_kw + "c"_kw;
  auto prefixed = "a"_kw + std::move(middleGroup);
  std::string prefixedInput = "abc!";
  auto prefixedResult = prefixed.terminal(prefixedInput);
  EXPECT_NE(prefixedResult, nullptr);
  EXPECT_EQ(prefixedResult - prefixedInput.c_str(), 3);

  auto startGroup = "a"_kw + "b"_kw;
  auto suffixed = std::move(startGroup) + "c"_kw;
  std::string suffixedInput = "abc!";
  auto suffixedResult = suffixed.terminal(suffixedInput);
  EXPECT_NE(suffixedResult, nullptr);
  EXPECT_EQ(suffixedResult - suffixedInput.c_str(), 3);
}

TEST(GroupTest, ExposesGrammarKind) {
  auto group = ":"_kw + ";"_kw;
  EXPECT_EQ(group.getKind(), pegium::grammar::ElementKind::Group);
}
