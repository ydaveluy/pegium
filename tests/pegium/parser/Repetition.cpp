#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <limits>
#include <memory>
#include <sstream>

using namespace pegium::parser;

namespace {

struct NoopNode : pegium::AstNode {};

} // namespace

TEST(RepetitionTest, OptionSomeAndManyBehaveAsExpected) {
  {
    std::string_view input = "";
    auto result = option("a"_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset, input.begin());
  }

  {
    std::string_view input = "";
    auto result = some("a"_kw).terminal(input);
    EXPECT_FALSE(result.IsValid());
  }

  {
    std::string_view input = "aaab";
    auto result = many("a"_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 3);
  }
}

/*TEST(RepetitionTest, ManyStopsWhenSubExpressionDoesNotConsumeInput) {
  auto expression = many(option("a"_kw));
  std::string_view input = "bbb";

  auto result = expression.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}*/

TEST(RepetitionTest, ParseRuleHandlesOptionalStarAndPlusVariants) {
  auto skipper = SkipperBuilder().build();

  {
    pegium::CstBuilder builder("b");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = option("a"_kw).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }

  {
    pegium::CstBuilder builder("aaab");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = many(":"_kw).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 0);
  }

  {
    pegium::CstBuilder builder("aaab");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = some(":"_kw).rule(ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 0);
  }

  {
    pegium::CstBuilder builder(":::b");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = many(":"_kw).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    pegium::CstBuilder builder(":::b");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = some(":"_kw).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    pegium::CstBuilder builder("bbb");
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};
    auto result = some(":"_kw).rule(ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}

/*TEST(RepetitionTest, ParseRuleStopsOnNonConsumingExpressions) {
  auto skipper = ContextBuilder().build();

  {
    pegium::CstBuilder builder("");
    const auto input = builder.getText();
    RecoverState ctx{builder, skipper};
    auto result = many(action<NoopNode>()).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }

  {
    pegium::CstBuilder builder("");
    const auto input = builder.getText();
    RecoverState ctx{builder, skipper};
    auto result = some(action<NoopNode>()).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}*/

TEST(RepetitionTest, FixedAndBoundedRepetitionsHandleLimits) {
  {
    std::string_view input = "aaab";
    auto result = rep<2>("a"_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 2);
  }

  {
    std::string_view input = "ab";
    auto result = rep<2>("a"_kw).terminal(input);
    EXPECT_FALSE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 1);
  }

  {
    std::string_view input = "aaab";
    auto result = rep<1, 3>("a"_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 3);
  }

  {
    std::string_view input = "a,a,a!";
    auto result = some("a"_kw, ","_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset - input.begin(), 5);
  }
}

/*TEST(RepetitionTest, BoundedParseRuleRespectsMinAndMax) {
  auto skipper = ContextBuilder().build();

  {
    pegium::CstBuilder builder(":::x");
    const auto input = builder.getText();
    RecoverState ctx{builder, skipper};
    auto result = rep<1, 3>(":"_kw).rule(ctx);
    EXPECT_TRUE(result);
    EXPECT_EQ(ctx.cursor() - input.begin(), 3);
  }

  {
    pegium::CstBuilder builder("x");
    const auto input = builder.getText();
    RecoverState ctx{builder, skipper};
    auto result = rep<1, 3>(":"_kw).rule(ctx);
    EXPECT_FALSE(result);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}*/

TEST(RepetitionTest, SeparatorHelpersAndPrintRepresentationsWork) {

  {
    std::string_view input = "";
    auto result = many("a"_kw, ","_kw).terminal(input);
    EXPECT_TRUE(result.IsValid());
    EXPECT_EQ(result.offset, input.begin());
  }

  std::ostringstream optionalText;
  optionalText << option("a"_kw);
  EXPECT_EQ(optionalText.str(), "'a'?");

  std::ostringstream manyText;
  manyText << many("a"_kw);
  EXPECT_EQ(manyText.str(), "'a'*");

  std::ostringstream someText;
  someText << some("a"_kw);
  EXPECT_EQ(someText.str(), "'a'+");

  std::ostringstream fixedText;
  fixedText << rep<2>("a"_kw);
  EXPECT_EQ(fixedText.str(), "'a'{2}");

  std::ostringstream openRangeText;
  openRangeText << rep<2, std::numeric_limits<std::size_t>::max()>("a"_kw);
  EXPECT_EQ(openRangeText.str(), "'a'{2,}");

  std::ostringstream boundedText;
  boundedText << rep<2, 4>("a"_kw);
  EXPECT_EQ(boundedText.str(), "'a'{2,4}");
}

/*TEST(RepetitionTest, FixedParseRuleCanFailAfterPartialConsumption) {
  auto skipper = ContextBuilder().build();
  pegium::CstBuilder builder(":x");
  const auto input = builder.getText();

  RecoverState ctx{builder, skipper};
  auto result = rep<2>(":"_kw).rule(ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 1);
}*/

/*TEST(RepetitionTest, BoundedRepetitionHandlesNonConsumingElementsSafely) {
  {
    auto expression = rep<1, 3>(option("a"_kw));
    std::string_view input = "bbb";

    auto terminal = expression.terminal(input);
    EXPECT_TRUE(terminal.IsValid());
    EXPECT_EQ(terminal.offset, input.begin());
  }

  {
    auto skipper = ContextBuilder().build();
    auto expression = rep<1, 3>(option("a"_kw));
    pegium::CstBuilder builder("bbb");
    const auto input = builder.getText();

    RecoverState ctx{builder, skipper};
    auto rule = expression.rule(ctx);
    EXPECT_TRUE(rule);
    EXPECT_EQ(ctx.cursor(), input.begin());
  }
}*/

TEST(RepetitionTest, RepetitionExposesBoundsAndUnderlyingElement) {
  auto expression = rep<2, 4>("a"_kw);
  const auto *grammarRep =
      dynamic_cast<const pegium::grammar::Repetition *>(std::addressof(expression));
  ASSERT_NE(grammarRep, nullptr);
  EXPECT_EQ(grammarRep->getKind(), pegium::grammar::ElementKind::Repetition);
  EXPECT_EQ(grammarRep->getMin(), 2u);
  EXPECT_EQ(grammarRep->getMax(), 4u);
  EXPECT_NE(grammarRep->getElement(), nullptr);

  std::string_view input = "aaaa!";
  auto result = expression.terminal(input.begin(), input.end());
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 4);
}
