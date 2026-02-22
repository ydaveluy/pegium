#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>

#include <memory>
#include <type_traits>

using namespace pegium::parser;

namespace {

struct DummyAstNode : pegium::AstNode {};

struct LeafNode : pegium::AstNode {
  string name;
};

struct RootNode : pegium::AstNode {
  pointer<LeafNode> leaf;
};

struct ChainNode : pegium::AstNode {
  string token;
  pointer<ChainNode> previous;
};

} // namespace

static_assert(std::is_same_v<decltype(
                                 std::declval<pegium::Reference<DummyAstNode> &>()
                                     .operator->()),
                             DummyAstNode *>);
static_assert(
    std::is_same_v<decltype(std::declval<const pegium::Reference<DummyAstNode> &>()
                                .operator->()),
                   const DummyAstNode *>);

TEST(ParserRuleTest, ParseRequiresFullConsumption) {
  ParserRule<DummyAstNode> rule{"Rule", ":"_kw + action<DummyAstNode>()};

  {
    auto result = rule.parse(":", ContextBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.len, 1u);
    EXPECT_TRUE(result.value != nullptr);
  }

  {
    auto result = rule.parse(":abc", ContextBuilder().build());
    EXPECT_FALSE(result.ret);
    EXPECT_EQ(result.len, 1u);
  }
}

TEST(ParserRuleTest, NestedParserRuleAssignmentSetsContainer) {
  ParserRule<LeafNode> leafRule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};
  ParserRule<RootNode> rootRule{"Root", assign<&RootNode::leaf>(leafRule)};

  auto result = rootRule.parse("leaf", ContextBuilder().build());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  ASSERT_TRUE(result.value->leaf != nullptr);
  EXPECT_EQ(result.value->leaf->name, "leaf");
  EXPECT_EQ(result.value->leaf->getContainer(), result.value.get());
}

TEST(ParserRuleTest, ParseGenericAndGetValueExposeTypedAstValue) {
  ParserRule<LeafNode> rule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};

  auto parsed = rule.parse("leaf", ContextBuilder().build());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.root_node != nullptr);

  auto node =
      detail::findFirstMatchingNode(*parsed.root_node, std::addressof(rule));
  ASSERT_TRUE(node.has_value());

  auto value = rule.getValue(*node);
  auto typed = std::dynamic_pointer_cast<LeafNode>(value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->name, "leaf");
  EXPECT_FALSE(rule.getTypeName().empty());

  auto generic = rule.parseGeneric("leaf", ContextBuilder().build());
  ASSERT_TRUE(generic.root_node != nullptr);
}

TEST(ParserRuleTest, DirectParserRuleCompositionUsesParserRuleChildValue) {
  ParserRule<LeafNode> inner{"Inner", assign<&LeafNode::name>("leaf"_kw)};
  ParserRule<LeafNode> outer{"Outer", inner};

  auto result = outer.parse("leaf", ContextBuilder().build());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  EXPECT_EQ(result.value->name, "leaf");
}

TEST(ParserRuleTest, RuleWithoutActionsBuildsDefaultAstNode) {
  ParserRule<DummyAstNode> rule{"Rule", ":"_kw};

  auto result = rule.parse(":", ContextBuilder().build());
  ASSERT_TRUE(result.ret);
  EXPECT_EQ(result.len, 1u);
  ASSERT_TRUE(result.value != nullptr);
}

TEST(ParserRuleTest, ParseRuleAddsNodeOnSuccessAndRewindsOnFailure) {
  ParserRule<LeafNode> rule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};
  auto context = ContextBuilder().build();

  {
    pegium::CstBuilder builder("leaf");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ok = rule.parse_rule(state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - input.begin(), 4);

    auto root = builder.finalize();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(rule));
  }

  {
    pegium::CstBuilder builder("xyz");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ko = rule.parse_rule(state);
    EXPECT_FALSE(ko);
    EXPECT_EQ(state.cursor(), input.begin());

    auto root = builder.finalize();
    EXPECT_EQ(root->begin(), root->end());
  }
}

TEST(ParserRuleTest, NewActionCreatesCurrentNodeAndAppliesQueuedAssignments) {
  ParserRule<ChainNode> rule{
      "Chain",
      assign<&ChainNode::token>("x"_kw) + action<ChainNode>() +
          action<ChainNode, &ChainNode::previous>()};

  auto result = rule.parse("x", ContextBuilder().build());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);

  EXPECT_TRUE(result.value->token.empty());
  ASSERT_TRUE(result.value->previous != nullptr);
  EXPECT_EQ(result.value->previous->token, "x");
  EXPECT_EQ(result.value->previous->getContainer(), result.value.get());
}

TEST(ParserRuleTest, InitActionBuildsImplicitCurrentWhenMissing) {
  ParserRule<ChainNode> rule{
      "Chain",
      assign<&ChainNode::token>("x"_kw) +
          action<ChainNode, &ChainNode::previous>()};

  auto result = rule.parse("x", ContextBuilder().build());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  ASSERT_TRUE(result.value->previous != nullptr);
  EXPECT_EQ(result.value->previous->token, "x");
  EXPECT_EQ(result.value->previous->getContainer(), result.value.get());
}
