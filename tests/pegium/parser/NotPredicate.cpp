#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(NotPredicateTest, SucceedsWhenSubExpressionDoesNotMatch) {
  auto predicate = !"a"_kw;
  std::string_view input = "bc";

  auto result = predicate.parse_terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(NotPredicateTest, FailsWhenSubExpressionMatches) {
  auto predicate = !"a"_kw;
  std::string_view input = "abc";

  auto result = predicate.parse_terminal(input);
  EXPECT_FALSE(result.IsValid());
}

TEST(NotPredicateTest, ParseRuleDoesNotAddNodes) {
  auto predicate = !"a"_kw;
  pegium::CstBuilder builder("bc");
  auto context = ContextBuilder().build();

  ParseState state{builder, context};
  auto result = predicate.parse_rule(state);
  EXPECT_TRUE(result);

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, ParseRuleFailsWhenSubExpressionMatchesAndRewinds) {
  auto predicate = !"a"_kw;
  pegium::CstBuilder builder("a:");
  const auto input = builder.getText();
  auto context = ContextBuilder().build();

  ParseState state{builder, context};
  auto result = predicate.parse_rule(state);
  EXPECT_FALSE(result);
  EXPECT_EQ(state.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(NotPredicateTest, ParseTerminalPointerOverloadWorks) {
  auto predicate = !"a"_kw;
  std::string_view input = "bc";

  auto result = predicate.parse_terminal(input.begin(), input.end());
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(NotPredicateTest, ExposesKindElementAndPrint) {
  auto predicate = !"a"_kw;
  EXPECT_EQ(predicate.getKind(), pegium::grammar::ElementKind::NotPredicate);
  EXPECT_NE(predicate.getElement(), nullptr);

  std::ostringstream os;
  os << predicate;
  EXPECT_EQ(os.str(), "!'a'");
}
