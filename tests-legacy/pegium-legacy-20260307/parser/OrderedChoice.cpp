#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

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
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto choice = (("a"_kw + "b"_kw) | "c"_kw).with_skipper(localSkipper);

  auto builderHarness = pegium::test::makeCstBuilderHarness("a   b");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, defaultSkipper};
  EXPECT_TRUE(parse(choice, ctx));
  EXPECT_EQ(ctx.cursorOffset(), 5U);
}

TEST(OrderedChoiceTest, WithLocalSkipperRestoresOuterSkipperAfterMatch) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto defaultSkipper = SkipperBuilder().build();
  auto choice = (("a"_kw + "b"_kw) | "c"_kw).with_skipper(localSkipper);
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

TEST(OrderedChoiceTest, WithLocalSkipperDoesNotFlattenPipeConcatenation) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto nested = (("a"_kw | "b"_kw).with_skipper(localSkipper)) | "c"_kw;

  std::ostringstream text;
  text << nested;
  EXPECT_EQ(text.str(), "(('a' | 'b') | 'c')");
}

TEST(OrderedChoiceTest, OperatorPipeCompositionsRemainFlattenedAndPrintable) {
  auto leftAssoc = ("a"_kw | "b"_kw) | "c"_kw;
  auto rightAssoc = "a"_kw | ("b"_kw | "c"_kw);
  auto extended = (("a"_kw | "b"_kw) | "c"_kw) | "d"_kw;

  {
    std::string input = "c!";
    auto result = leftAssoc.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - input.c_str(), 1);
  }
  {
    std::string input = "b!";
    auto result = rightAssoc.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - input.c_str(), 1);
  }
  {
    std::string input = "d!";
    auto result = extended.terminal(input);
    EXPECT_NE(result, nullptr);
    EXPECT_EQ(result - input.c_str(), 1);
  }

  std::ostringstream leftText;
  leftText << leftAssoc;
  EXPECT_EQ(leftText.str(), "('a' | 'b' | 'c')");

  std::ostringstream mergedText;
  mergedText << extended;
  EXPECT_EQ(mergedText.str(), "('a' | 'b' | 'c' | 'd')");
}

TEST(OrderedChoiceTest, LvaluePipeCompositionsAreFlattened) {
  auto left = "a"_kw | "b"_kw;
  auto right = "c"_kw | "d"_kw;
  const auto constRight = "e"_kw | "f"_kw;

  std::ostringstream lhsLvalue;
  lhsLvalue << (left | "e"_kw);
  EXPECT_EQ(lhsLvalue.str(), "('a' | 'b' | 'e')");

  std::ostringstream rhsLvalue;
  rhsLvalue << ("e"_kw | right);
  EXPECT_EQ(rhsLvalue.str(), "('e' | 'c' | 'd')");

  std::ostringstream bothLvalue;
  bothLvalue << (left | right);
  EXPECT_EQ(bothLvalue.str(), "('a' | 'b' | 'c' | 'd')");

  std::ostringstream rvalueLhsConstRhs;
  rvalueLhsConstRhs << (("a"_kw | "b"_kw) | constRight);
  EXPECT_EQ(rvalueLhsConstRhs.str(), "('a' | 'b' | 'e' | 'f')");
}

TEST(OrderedChoiceTest, WithSkipperPipeCompositionsStayNestedOnBothSides) {
  TerminalRule<> ws{"WS", some(s)};
  auto localSkipper = SkipperBuilder().ignore(ws).build();
  auto plain = "c"_kw;
  auto withSkipper = ("a"_kw | "b"_kw).with_skipper(localSkipper);

  std::ostringstream withSkipperLeft;
  withSkipperLeft << (withSkipper | plain);
  EXPECT_EQ(withSkipperLeft.str(), "(('a' | 'b') | 'c')");

  std::ostringstream withSkipperRight;
  withSkipperRight << (plain | withSkipper);
  EXPECT_EQ(withSkipperRight.str(), "('c' | ('a' | 'b'))");
}

TEST(OrderedChoiceTest, ParseTerminalPointerOverloadWorks) {
  auto choice = "ab"_kw | "a"_kw;
  std::string input = "abx";

  auto result = choice.terminal(input);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.c_str(), 2);
}

TEST(OrderedChoiceTest, ExplicitOperatorPipeOverloadsPreserveAlternatives) {
  auto rightChoice = "b"_kw | "c"_kw;
  auto prefixed = "a"_kw | std::move(rightChoice);
  std::string prefixedInput = "c!";
  auto prefixedResult = prefixed.terminal(prefixedInput);
  EXPECT_NE(prefixedResult, nullptr);
  EXPECT_EQ(prefixedResult - prefixedInput.c_str(), 1);

  auto leftChoice = "a"_kw | "b"_kw;
  auto suffixed = std::move(leftChoice) | "c"_kw;
  std::string suffixedInput = "c!";
  auto suffixedResult = suffixed.terminal(suffixedInput);
  EXPECT_NE(suffixedResult, nullptr);
  EXPECT_EQ(suffixedResult - suffixedInput.c_str(), 1);
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

TEST(OrderedChoiceTest, ExposesGrammarKind) {
  auto choice = "a"_kw | "b"_kw;
  EXPECT_EQ(choice.getKind(), pegium::grammar::ElementKind::OrderedChoice);
}
