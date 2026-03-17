#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(NotPredicateTest, SucceedsWhenSubExpressionDoesNotMatch) {
  auto predicate = !"a"_kw;
  std::string input = "bc";

  auto result = predicate.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(NotPredicateTest, FailsWhenSubExpressionMatches) {
  auto predicate = !"a"_kw;
  std::string input = "abc";

  auto result = predicate.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(NotPredicateTest, ParseRuleDoesNotAddNodes) {
  auto predicate = !"a"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("bc");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(predicate, ctx);
  EXPECT_TRUE(result);

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, ParseRuleFailsWhenSubExpressionMatchesAndRewinds) {
  auto predicate = !"a"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("a:");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(predicate, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, ParseTerminalPointerOverloadWorks) {
  auto predicate = !"a"_kw;
  std::string input = "bc";

  auto result = predicate.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(NotPredicateTest, ExposesKindElementAndPrint) {
  auto predicate = !"a"_kw;
  EXPECT_EQ(predicate.getKind(), pegium::grammar::ElementKind::NotPredicate);
  EXPECT_NE(predicate.getElement(), nullptr);

  std::ostringstream os;
  os << predicate;
  EXPECT_EQ(os.str(), "!'a'");
}
