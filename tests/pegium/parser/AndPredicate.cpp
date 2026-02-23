#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <sstream>

using namespace pegium::parser;

TEST(AndPredicateTest, LookaheadSucceedsWithoutConsumingInput) {
  auto lookahead = &":"_kw;
  std::string_view input = ":abc";

  auto result = lookahead.terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(AndPredicateTest, LookaheadFailsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  std::string_view input = "xabc";

  auto result = lookahead.terminal(input);
  EXPECT_FALSE(result.IsValid());
}

TEST(AndPredicateTest, ParseRuleDoesNotModifyParentNode) {
  auto lookahead = &":"_kw;
  pegium::CstBuilder builder(":abc");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = lookahead.rule(ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, ParseRuleFailsAndRewindsWhenSubExpressionFails) {
  auto lookahead = &":"_kw;
  pegium::CstBuilder builder("xabc");
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = lookahead.rule(ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(AndPredicateTest, ParseTerminalPointerOverloadWorks) {
  auto lookahead = &":"_kw;
  std::string_view input = ":abc";

  auto result = lookahead.terminal(input.begin(), input.end());
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());
}

TEST(AndPredicateTest, ExposesKindElementAndPrint) {
  auto lookahead = &":"_kw;
  EXPECT_EQ(lookahead.getKind(), pegium::grammar::ElementKind::AndPredicate);
  EXPECT_NE(lookahead.getElement(), nullptr);

  std::ostringstream os;
  os << lookahead;
  EXPECT_EQ(os.str(), "&':'");
}
