#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(AndPredicateTest, LookaheadSucceedsWithoutConsumingInput) {
  auto lookahead = &":"_kw;
  std::string input = ":abc";

  auto result = lookahead.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(AndPredicateTest, LookaheadFailsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  std::string input = "xabc";

  auto result = lookahead.terminal(input);
  EXPECT_EQ(result, nullptr);
}

TEST(AndPredicateTest, ParseRuleDoesNotModifyParentNode) {
  auto lookahead = &":"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness(":abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(lookahead, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, ParseRuleFailsAndRewindsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  auto builderHarness = pegium::test::makeCstBuilderHarness("xabc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(lookahead, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, ParseTerminalPointerOverloadWorks) {
  auto lookahead = &":"_kw;
  std::string input = ":abc";

  auto result = lookahead.terminal(input);
  EXPECT_EQ(result, input.c_str());
}

TEST(AndPredicateTest, ExposesKindElementAndPrint) {
  auto lookahead = &":"_kw;
  EXPECT_EQ(lookahead.getKind(), pegium::grammar::ElementKind::AndPredicate);
  EXPECT_NE(lookahead.getElement(), nullptr);

  std::ostringstream os;
  os << lookahead;
  EXPECT_EQ(os.str(), "&':'");
}
