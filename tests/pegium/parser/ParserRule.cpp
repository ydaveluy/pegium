#include <gtest/gtest.h>
#include <pegium/ParseJsonTestSupport.hpp>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>

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

template <typename RuleType>
auto parse_rule(const RuleType &rule, std::string_view text,
                const Skipper &skipper = SkipperBuilder().build(),
                const ParseOptions &options = {}) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  pegium::test::parse_rule(rule, *document, skipper, options);
  return document;
}

} // namespace

static_assert(
    std::is_same_v<decltype(std::declval<pegium::Reference<DummyAstNode> &>()
                                .operator->()),
                   DummyAstNode *>);
static_assert(std::is_same_v<
              decltype(std::declval<const pegium::Reference<DummyAstNode> &>()
                           .operator->()),
              const DummyAstNode *>);

TEST(ParserRuleTest, ParseRequiresFullConsumption) {
  ParserRule<DummyAstNode> rule{"Rule", ":"_kw + create<DummyAstNode>()};

  {
    auto document = pegium::test::ExpectAst(rule, ":",
                                            R"json({
  "$type": "DummyAstNode"
})json");
    auto &result = document->parseResult;
    EXPECT_TRUE(result.fullMatch);
  }

  {
    auto document = parse_rule(rule, ":abc");
    auto &result = document->parseResult;
    EXPECT_FALSE(result.fullMatch);
    EXPECT_TRUE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
  }
}

TEST(ParserRuleTest, NestedParserRuleAssignmentSetsContainer) {
  ParserRule<LeafNode> leafRule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};
  ParserRule<RootNode> rootRule{"Root", assign<&RootNode::leaf>(leafRule)};

  auto document = pegium::test::ExpectAst(rootRule, "leaf",
                                          R"json({
  "$type": "RootNode",
  "leaf": {
    "$type": "LeafNode",
    "name": "leaf"
  }
})json");
  auto *typed = pegium::ast_ptr_cast<RootNode>(document->parseResult.value);
  ASSERT_TRUE(typed != nullptr);
  ASSERT_TRUE(typed->leaf != nullptr);
  EXPECT_EQ(typed->leaf->getContainer(), typed);
}

TEST(ParserRuleTest, ParseAndGetValueExposeTypedAstValue) {
  ParserRule<LeafNode> rule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};

  auto document = pegium::test::ExpectAst(rule, "leaf",
                                          R"json({
  "$type": "LeafNode",
  "name": "leaf"
})json");
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.cst != nullptr);

  auto node = detail::findFirstMatchingNode(*parsed.cst, std::addressof(rule));
  ASSERT_TRUE(node.has_value());

  auto value = rule.getValue(*node);
  auto *typed = pegium::ast_ptr_cast<LeafNode>(value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->name, "leaf");
  EXPECT_FALSE(rule.getTypeName().empty());
}

TEST(ParserRuleTest, DirectParserRuleCompositionUsesParserRuleChildValue) {
  ParserRule<LeafNode> inner{"Inner", assign<&LeafNode::name>("leaf"_kw)};
  ParserRule<LeafNode> outer{"Outer", inner};

  pegium::test::ExpectAst(outer, "leaf",
                          R"json({
  "$type": "LeafNode",
  "name": "leaf"
})json");
}

TEST(ParserRuleTest, RuleWithoutActionsBuildsDefaultAstNode) {
  ParserRule<DummyAstNode> rule{"Rule", ":"_kw};

  pegium::test::ExpectAst(rule, ":", "{}", {.includeType = false});
}

TEST(ParserRuleTest, ParseRuleAddsNodeOnSuccessAndKeepsLocalFailureState) {
  ParserRule<LeafNode> rule{"Leaf", assign<&LeafNode::name>("leaf"_kw)};
  auto skipper = SkipperBuilder().build();

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("leaf");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};

    auto ok = parse(rule, ctx);
    EXPECT_TRUE(ok);
    EXPECT_EQ(ctx.cursor() - input.begin(), 4);

    auto root = builder.getRootCstNode();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(rule));
  }

  {
    auto builderHarness = pegium::test::makeCstBuilderHarness("xyz");
    auto &builder = builderHarness.builder;
    const auto input = builder.getText();
    ParseContext ctx{builder, skipper};

    auto ko = parse(rule, ctx);
    EXPECT_FALSE(ko);
    EXPECT_EQ(ctx.cursor(), input.begin());

    auto root = builder.getRootCstNode();
    EXPECT_NE(root->begin(), root->end());
  }
}

TEST(ParserRuleTest, TopLevelParseFailureLeavesEmptyCst) {
  ParserRule<DummyAstNode> rule{"Rule", "service"_kw};

  auto document = parse_rule(rule, "");
  ASSERT_NE(document->parseResult.cst, nullptr);
  EXPECT_EQ(document->parseResult.cst->begin(), document->parseResult.cst->end());
  EXPECT_FALSE(document->parseSucceeded());
}

TEST(ParserRuleTest, ConstructorOptionSetsLocalSkipper) {
  TerminalRule<> ws{"WS", some(s)};
  DataTypeRule<std::string> token{"Token", "a"_kw + "b"_kw};
  ParseOptions strictNoRecovery;
  strictNoRecovery.recoveryEnabled = false;

  ParserRule<LeafNode> withoutLocalSkipper{"Leaf",
                                           assign<&LeafNode::name>(token)};
  auto document = parse_rule(withoutLocalSkipper, "a   b",
                             SkipperBuilder().build(), strictNoRecovery);
  auto &baseline = document->parseResult;
  EXPECT_FALSE(baseline.value);

  ParserRule<LeafNode> withLocalSkipper{
      "Leaf", assign<&LeafNode::name>(token),
      opt::with_skipper(SkipperBuilder().ignore(ws).build())};

  pegium::test::ExpectAst(withLocalSkipper, "a   b",
                          R"json({
  "$type": "LeafNode",
  "name": "ab"
})json",
                          strictNoRecovery);
}

TEST(ParserRuleTest, NewActionCreatesCurrentNodeAndAppliesQueuedAssignments) {
  ParserRule<ChainNode> rule{
      "Chain", assign<&ChainNode::token>("x"_kw) + create<ChainNode>() +
                   nest<ChainNode, &ChainNode::previous>()};

  auto document = parse_rule(rule, "x");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<ChainNode>(result.value);
  ASSERT_TRUE(typed != nullptr);

  EXPECT_TRUE(typed->token.empty());
  ASSERT_TRUE(typed->previous != nullptr);
  EXPECT_EQ(typed->previous->token, "x");
  EXPECT_EQ(typed->previous->getContainer(), typed);
}

TEST(ParserRuleTest, InitActionBuildsImplicitCurrentWhenMissing) {
  ParserRule<ChainNode> rule{"Chain",
                             assign<&ChainNode::token>("x"_kw) +
                                 nest<ChainNode, &ChainNode::previous>()};

  auto document = parse_rule(rule, "x");
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<ChainNode>(result.value);
  ASSERT_TRUE(typed != nullptr);
  ASSERT_TRUE(typed->previous != nullptr);
  EXPECT_EQ(typed->previous->token, "x");
  EXPECT_EQ(typed->previous->getContainer(), typed);
}
