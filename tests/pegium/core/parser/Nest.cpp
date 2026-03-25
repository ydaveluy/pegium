#include <gtest/gtest.h>
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {};

struct ParentNode : pegium::AstNode {
  pointer<ChildNode> child;
};

} // namespace

TEST(NestTest, AssignsCurrentValueToFeature) {
  auto initParent = nest<&ParentNode::child>();
  auto current = std::make_unique<ChildNode>();

  EXPECT_EQ(initParent.getKind(), pegium::grammar::ElementKind::Nest);

  auto created = initParent.getValue(std::move(current));
  auto *parent = pegium::ast_ptr_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_NE(parent->child.get(), nullptr);
  EXPECT_EQ(parent->child->getContainer(), parent);
}

TEST(NestTest, ExplicitFeatureTemplateAssignsCurrentValue) {
  auto initParent = nest<ParentNode, &ParentNode::child>();
  auto current = std::make_unique<ChildNode>();

  auto created = initParent.getValue(std::move(current));
  auto *parent = pegium::ast_ptr_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_NE(parent->child.get(), nullptr);
  EXPECT_EQ(parent->child->getContainer(), parent);
}
