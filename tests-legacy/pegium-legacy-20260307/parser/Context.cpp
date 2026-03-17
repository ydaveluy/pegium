#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <memory>

using namespace pegium::parser;

namespace {

struct StringNode : pegium::AstNode {
  string value;
};

bool containsHiddenNode(const pegium::CstNodeView &node) {
  if (node.isHidden()) {
    return true;
  }
  for (const auto &child : node) {
    if (containsHiddenNode(child)) {
      return true;
    }
  }
  return false;
}

bool containsHiddenNode(const pegium::RootCstNode &root) {
  for (const auto &child : root) {
    if (containsHiddenNode(child)) {
      return true;
    }
  }
  return false;
}

} // namespace

TEST(ContextTest, DefaultContextSkipsNothing) {
  auto skipper = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  auto result = skipper.skip(input.begin(), builder);
  EXPECT_EQ(result, input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreSkipsWhitespaceBeforeParsing) {
  TerminalRule<> ws{"WS", some(s)};
  auto skipper = SkipperBuilder().ignore(ws).build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("   abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();

  auto result = skipper.skip(input.begin(), builder);
  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 3);

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreAndHideAreApplied) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comment{"COMMENT", "//"_kw <=> &(eol | eof)};
  DataTypeRule<std::string> rule{"Rule", "a"_kw + "b"_kw};
  ParserRule<StringNode> root{"Root", assign<&StringNode::value>(rule)};

  pegium::workspace::Document document;
  document.setText("a// comment\n   b");
  root.parse(document, SkipperBuilder().ignore(ws).hide(comment).build());
  const auto &result = document.parseResult;

  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<StringNode>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, "ab");
  ASSERT_TRUE(result.cst);
  EXPECT_TRUE(containsHiddenNode(*result.cst));
}

TEST(ContextTest, IgnoreWithTwoRulesUsesTwoElementFastPath) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipper = SkipperBuilder().ignore(ws, comma).build();

  auto builderHarness = pegium::test::makeCstBuilderHarness(",   abc");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 4);

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, HideWithThreeRulesUsesGenericPathAndKeepsMatchedRule) {
  TerminalRule<> colon{"Colon", ":"_kw};
  TerminalRule<> semicolon{"Semicolon", ";"_kw};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipper = SkipperBuilder().hide(colon, semicolon, comma).build();

  auto builderHarness = pegium::test::makeCstBuilderHarness(",x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 1);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_TRUE((*it).isHidden());
  EXPECT_EQ((*it).getText(), ",");
  EXPECT_EQ((*it).getGrammarElement(), std::addressof(comma));
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(ContextTest, SkipHiddenNodesAtEndReturnsImmediately) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> colon{"Colon", ":"_kw};
  auto skipper = SkipperBuilder().ignore(ws).hide(colon).build();

  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_EQ(result, input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, ContextCanBeConvertedToParseContextWithoutOwning) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipperContext = SkipperContext{std::tie(comma), std::tie(ws)};
  Skipper skipper = skipperContext;

  auto builderHarness = pegium::test::makeCstBuilderHarness(" ,x");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = skipper.skip(input.begin(), builder);

  EXPECT_NE(result, nullptr);
  EXPECT_EQ(result - input.begin(), 2);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_TRUE((*it).isHidden());
  EXPECT_EQ((*it).getText(), ",");
}
