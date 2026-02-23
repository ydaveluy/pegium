#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <memory>

using namespace pegium::parser;

namespace {

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
  pegium::CstBuilder builder("abc");
  const auto input = builder.getText();

  auto result = skipper.skipHiddenNodes(input, builder);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreSkipsWhitespaceBeforeParsing) {
  TerminalRule<> ws{"WS", some(s)};
  auto skipper = SkipperBuilder().ignore(ws).build();
  pegium::CstBuilder builder("   abc");
  const auto input = builder.getText();

  auto result = skipper.skipHiddenNodes(input, builder);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 3);

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, IgnoreAndHideAreApplied) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comment{"COMMENT", "//"_kw <=> &(eol | eof)};
  DataTypeRule<std::string> rule{"Rule", "a"_kw + "b"_kw};

  auto result = rule.parse("a// comment\n   b",
                           SkipperBuilder().ignore(ws).hide(comment).build());

  ASSERT_TRUE(result.ret);
  EXPECT_EQ(result.value, "ab");
  ASSERT_TRUE(result.root_node);
  EXPECT_TRUE(containsHiddenNode(*result.root_node));
}

TEST(ContextTest, IgnoreWithTwoRulesUsesTwoElementFastPath) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipper = SkipperBuilder().ignore(ws, comma).build();

  pegium::CstBuilder builder(",   abc");
  const auto input = builder.getText();
  auto result = skipper.skipHiddenNodes(input, builder);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 4);

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, HideWithThreeRulesUsesGenericPathAndKeepsMatchedRule) {
  TerminalRule<> colon{"Colon", ":"_kw};
  TerminalRule<> semicolon{"Semicolon", ";"_kw};
  TerminalRule<> comma{"Comma", ","_kw};
  auto skipper = SkipperBuilder().hide(colon, semicolon, comma).build();

  pegium::CstBuilder builder(",x");
  const auto input = builder.getText();
  auto result = skipper.skipHiddenNodes(input, builder);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 1);

  auto root = builder.finalize();
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

  pegium::CstBuilder builder("");
  const auto input = builder.getText();
  auto result = skipper.skipHiddenNodes(input, builder);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset, input.begin());

  auto root = builder.finalize();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(ContextTest, ContextCanBeConvertedToParseContextWithoutOwning) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> comma{"Comma", ","_kw};
  auto typedContext = Context{std::tie(comma), std::tie(ws)};
  Skipper parseContext = typedContext;

  pegium::CstBuilder builder(" ,x");
  const auto input = builder.getText();
  auto result = parseContext.skipHiddenNodes(input, builder);

  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 2);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_TRUE((*it).isHidden());
  EXPECT_EQ((*it).getText(), ",");
}

TEST(ContextTest, DefaultRecoveryPolicyForceInsertsSinglePunctuationLiteral) {
  auto skipper = SkipperBuilder().build();
  TerminalRule<> id{"ID", some(w)};
  const auto comma = ","_kw;
  const auto openParen = "("_kw;
  const auto closeParen = ")"_kw;
  const auto keyword = "service"_kw;

  pegium::CstBuilder builder("x");
  const auto input = builder.getText();

  EXPECT_TRUE(skipper.canForceInsert(std::addressof(id), input.begin(),
                                     input.end()));
  EXPECT_TRUE(skipper.canForceInsert(std::addressof(comma), input.begin(),
                                     input.end()));
  EXPECT_FALSE(skipper.canForceInsert(std::addressof(openParen), input.begin(),
                                      input.end()));
  EXPECT_TRUE(skipper.canForceInsert(std::addressof(closeParen), input.begin(),
                                     input.end()));
  EXPECT_FALSE(skipper.canForceInsert(std::addressof(keyword), input.begin(),
                                      input.end()));
}
