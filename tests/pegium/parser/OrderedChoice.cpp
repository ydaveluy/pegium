#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(OrderedChoiceTest, ChoosesFirstMatchingAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  std::string_view input = "abx";

  auto result = choice.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(OrderedChoiceTest, FallsBackToNextAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  std::string_view input = "ax";

  auto result = choice.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 1);
}

TEST(OrderedChoiceTest, ParseRuleAddsNodesFromSelectedAlternative) {
  auto choice = "ab"_kw | "a"_kw;
  pegium::CstBuilder builder("a");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = choice.rule(ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(OrderedChoiceTest, ParseTerminalFailsWhenNoAlternativeMatches) {
  auto choice = "ab"_kw | "cd"_kw;
  std::string_view input = "xy";

  auto result = choice.terminal(input);
  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(OrderedChoiceTest, ParseRuleRewindsBeforeTryingNextAlternative) {
  auto choice = (":"_kw + ";"_kw) | ":"_kw;
  pegium::CstBuilder builder(":x");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = choice.rule(ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), ":");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(OrderedChoiceTest, OperatorPipeCompositionsRemainFlattenedAndPrintable) {
  auto leftAssoc = ("a"_kw | "b"_kw) | "c"_kw;
  auto rightAssoc = "a"_kw | ("b"_kw | "c"_kw);
  auto extended = (("a"_kw | "b"_kw) | "c"_kw) | "d"_kw;

  {
    std::string_view input = "c!";
    auto result = leftAssoc.terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 1);
  }
  {
    std::string_view input = "b!";
    auto result = rightAssoc.terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 1);
  }
  {
    std::string_view input = "d!";
    auto result = extended.terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 1);
  }

  std::ostringstream leftText;
  leftText << leftAssoc;
  EXPECT_EQ(leftText.str(), "('a' | 'b' | 'c')");

  std::ostringstream mergedText;
  mergedText << extended;
  EXPECT_EQ(mergedText.str(), "('a' | 'b' | 'c' | 'd')");
}

TEST(OrderedChoiceTest, ParseTerminalPointerOverloadWorks) {
  auto choice = "ab"_kw | "a"_kw;
  std::string_view input = "abx";

  auto result = choice.terminal(input.begin(), input.end());
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);
}

TEST(OrderedChoiceTest, ExplicitOperatorPipeOverloadsPreserveAlternatives) {
  auto rightChoice = "b"_kw | "c"_kw;
  auto prefixed = "a"_kw | std::move(rightChoice);
  std::string_view prefixedInput = "c!";
  auto prefixedResult = prefixed.terminal(prefixedInput);
  EXPECT_TRUE(prefixedResult.IsValid());
  EXPECT_EQ(prefixedResult.offset - prefixedInput.begin(), 1);

  auto leftChoice = "a"_kw | "b"_kw;
  auto suffixed = std::move(leftChoice) | "c"_kw;
  std::string_view suffixedInput = "c!";
  auto suffixedResult = suffixed.terminal(suffixedInput);
  EXPECT_TRUE(suffixedResult.IsValid());
  EXPECT_EQ(suffixedResult.offset - suffixedInput.begin(), 1);
}

TEST(OrderedChoiceTest, ParseRuleFailureRewindsCursorAndLeavesNoNodes) {
  auto choice = ("a"_kw + "b"_kw) | ("c"_kw + "d"_kw);
  pegium::CstBuilder builder("ax");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = choice.rule(ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(OrderedChoiceTest, ExposesGrammarKind) {
  auto choice = "a"_kw | "b"_kw;
  EXPECT_EQ(choice.getKind(), pegium::grammar::ElementKind::OrderedChoice);
}
