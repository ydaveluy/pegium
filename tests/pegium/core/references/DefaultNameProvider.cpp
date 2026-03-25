#include <gtest/gtest.h>

#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/references/DefaultNameProvider.hpp>

using namespace pegium::parser;

namespace pegium::references {
namespace {

struct NamedNode : pegium::NamedAstNode {};

struct LegacyNamedNode : pegium::AstNode {
  string name;
};

struct NumericNamedNode : pegium::AstNode {
  int32_t name = 0;
};

struct UnnamedNode : pegium::AstNode {
  string id;
};

template <typename RuleType>
ParseResult parse_rule(const RuleType &rule, std::string_view text,
                       const Skipper &skipper = SkipperBuilder().build()) {
  return pegium::test::parse_rule_result(rule, text, skipper);
}

TEST(DefaultNameProviderTest, ReturnsAstNameWithoutCstForNamedAstNode) {
  NamedNode node;
  node.name = "direct";

  DefaultNameProvider provider;
  const auto name = provider.getName(node);
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "direct");
  EXPECT_FALSE(provider.getNameNode(node).has_value());
}

TEST(DefaultNameProviderTest, ReturnsNameAndNameNodeForNamedAstNode) {
  ParserRule<NamedNode> rule{"Named", assign<&NamedNode::name>("value"_kw)};

  auto result = parse_rule(rule, "value");
  ASSERT_TRUE(result.value);
  auto *node = pegium::ast_ptr_cast<NamedNode>(result.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  const auto name = provider.getName(*node);
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "value");

  const auto nameNode = provider.getNameNode(*node);
  ASSERT_TRUE(nameNode.has_value());
  EXPECT_EQ(nameNode->getText(), "value");
}

TEST(DefaultNameProviderTest, FallsBackToNameAssignmentForLegacyAstNode) {
  ParserRule<LegacyNamedNode> rule{
      "Legacy", assign<&LegacyNamedNode::name>("value"_kw)};

  auto result = parse_rule(rule, "value");
  ASSERT_TRUE(result.value);
  auto *node = pegium::ast_ptr_cast<LegacyNamedNode>(result.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  const auto name = provider.getName(*node);
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(*name, "value");

  const auto nameNode = provider.getNameNode(*node);
  ASSERT_TRUE(nameNode.has_value());
  EXPECT_EQ(nameNode->getText(), "value");
}

TEST(DefaultNameProviderTest, ReturnsNulloptWhenNameFeatureIsMissing) {
  ParserRule<UnnamedNode> rule{"Unnamed", assign<&UnnamedNode::id>("value"_kw)};

  auto result = parse_rule(rule, "value");
  ASSERT_TRUE(result.value);
  auto *node = pegium::ast_ptr_cast<UnnamedNode>(result.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  EXPECT_FALSE(provider.getName(*node).has_value());
  EXPECT_FALSE(provider.getNameNode(*node).has_value());
}

TEST(DefaultNameProviderTest, ReturnsNulloptWhenNameValueIsNotStringBacked) {
  ParserRule<NumericNamedNode> rule{
      "Named",
      assign<&NumericNamedNode::name>(TerminalRule<int32_t>{"INT", some(d)})};

  auto result = parse_rule(rule, "42");
  ASSERT_TRUE(result.value);
  auto *node = pegium::ast_ptr_cast<NumericNamedNode>(result.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  EXPECT_FALSE(provider.getName(*node).has_value());
  const auto nameNode = provider.getNameNode(*node);
  ASSERT_TRUE(nameNode.has_value());
  EXPECT_EQ(nameNode->getText(), "42");
}

TEST(DefaultNameProviderTest, ReturnsNulloptWhenStringValueIsEmpty) {
  TerminalRule<std::string> ID{
      "ID", "a-zA-Z_"_cr + many(w),
      opt::with_converter([](std::string_view) noexcept
                              -> opt::ConversionResult<std::string> {
        return opt::conversion_value<std::string>("");
      })};
  ParserRule<NamedNode> rule{"Named", assign<&NamedNode::name>(ID)};

  auto result = parse_rule(rule, "value");
  ASSERT_TRUE(result.value);
  auto *node = pegium::ast_ptr_cast<NamedNode>(result.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  EXPECT_FALSE(provider.getName(*node).has_value());
  const auto nameNode = provider.getNameNode(*node);
  ASSERT_TRUE(nameNode.has_value());
  EXPECT_EQ(nameNode->getText(), "value");
}

} // namespace
} // namespace pegium::references
