#include <gtest/gtest.h>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/syntax-tree/CstUtils.hpp>
#include <pegium/workspace/Document.hpp>

namespace {

using namespace pegium::parser;

struct LeafNode : pegium::AstNode {
  string name;
};

struct RootNode : pegium::AstNode {
  pointer<LeafNode> leaf;
};

struct TokenNode : pegium::AstNode {};

template <typename RuleType>
std::unique_ptr<pegium::workspace::Document>
parse_rule(const RuleType &rule, std::string_view text,
           const Skipper &skipper = SkipperBuilder().build()) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  rule.parse(*document, skipper);
  return document;
}

} // namespace

TEST(SyntaxTreeTest, CstUtilsHandlesInvalidNodeViews) {
  const pegium::CstNodeView invalid{};
  EXPECT_FALSE(pegium::is_valid(invalid));
  EXPECT_EQ(pegium::cst_begin(invalid), 0u);
  EXPECT_EQ(pegium::cst_end(invalid), 0u);
  EXPECT_FALSE(pegium::find_node_for_feature(invalid, "name").has_value());
  EXPECT_FALSE(pegium::find_node_for_keyword(invalid, "kw").has_value());
  EXPECT_FALSE(pegium::find_name_like_node(invalid, "id").has_value());
}

TEST(SyntaxTreeTest, CstUtilsFindsFeatureNodesForAssignments) {
  ParserRule<LeafNode> leafRule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};
  ParserRule<RootNode> rootRule{"Root", assign<&RootNode::leaf>(leafRule)};

  auto parsed = parse_rule(rootRule, "leaf", SkipperBuilder().build());
  auto &result = parsed->parseResult;
  ASSERT_TRUE(result.value != nullptr);
  auto *typed = pegium::ast_ptr_cast<RootNode>(result.value);
  ASSERT_TRUE(typed != nullptr);
  ASSERT_TRUE(typed->leaf != nullptr);

  const auto match =
      pegium::find_node_for_feature(typed->leaf->getCstNode(), "name");
  ASSERT_TRUE(match.has_value());
  EXPECT_EQ(match->getText(), "leaf");

  const auto matches =
      pegium::find_nodes_for_feature(typed->leaf->getCstNode(), "name");
  ASSERT_EQ(matches.size(), 1u);
  EXPECT_EQ(matches.front().getText(), "leaf");

  EXPECT_FALSE(
      pegium::find_node_for_feature(typed->leaf->getCstNode(), "missing")
          .has_value());
}

TEST(SyntaxTreeTest, CstUtilsFindsKeywordNodesByIndex) {
  ParserRule<TokenNode> rule{"Rule", "a"_kw + "a"_kw};
  auto parsed = parse_rule(rule, "a a", SkipperBuilder().build());
  auto &result = parsed->parseResult;
  ASSERT_TRUE(result.value != nullptr);

  const auto first =
      pegium::find_node_for_keyword(result.value->getCstNode(), "a", 0u);
  const auto second =
      pegium::find_node_for_keyword(result.value->getCstNode(), "a", 1u);
  const auto third =
      pegium::find_node_for_keyword(result.value->getCstNode(), "a", 2u);

  ASSERT_TRUE(first.has_value());
  ASSERT_TRUE(second.has_value());
  EXPECT_EQ(first->getText(), "a");
  EXPECT_EQ(second->getText(), "a");
  EXPECT_FALSE(third.has_value());
}

TEST(SyntaxTreeTest, CstUtilsFindsNameLikeLeafInSubtree) {
  ParserRule<LeafNode> rule{"Leaf", assign<&LeafNode::name>("catalogue"_kw)};
  auto parsed = parse_rule(rule, "catalogue", SkipperBuilder().build());
  auto &result = parsed->parseResult;
  ASSERT_TRUE(result.value != nullptr);
  auto *typed = pegium::ast_ptr_cast<LeafNode>(result.value);
  ASSERT_TRUE(typed != nullptr);

  const auto found =
      pegium::find_name_like_node(typed->getCstNode(), "catalogue");
  ASSERT_TRUE(found.has_value());
  EXPECT_EQ(found->getText(), "catalogue");

  EXPECT_FALSE(pegium::find_name_like_node(typed->getCstNode(), "missing")
                   .has_value());
}
