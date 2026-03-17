#include <gtest/gtest.h>

#include <pegium/TestRuleParser.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/references/DefaultNameProvider.hpp>
#include <pegium/workspace/Document.hpp>

using namespace pegium::parser;

namespace pegium::references {
namespace {

struct NamedNode : pegium::AstNode {
  string name;
};

template <typename RuleType>
auto parse_rule(const RuleType &rule, std::string_view text,
                const Skipper &skipper = SkipperBuilder().build()) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  pegium::test::parse_rule(rule, *document, skipper);
  return std::move(document);
}

TEST(DefaultNameProviderTest, ReturnsNameAndNameNodeForNamedAstNode) {
  ParserRule<NamedNode> rule{"Named", assign<&NamedNode::name>("value"_kw)};

  auto document = parse_rule(rule, "value");
  ASSERT_TRUE(document->parseResult.value);
  auto *node = pegium::ast_ptr_cast<NamedNode>(document->parseResult.value);
  ASSERT_NE(node, nullptr);

  DefaultNameProvider provider;
  EXPECT_EQ(provider.getName(*node), "value");

  const auto nameNode = provider.getNameNode(*node);
  ASSERT_TRUE(nameNode.valid());
  EXPECT_EQ(nameNode.getText(), "value");
}

} // namespace
} // namespace pegium::references
