#include <gtest/gtest.h>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
using namespace pegium::parser;

namespace {

struct ChildNode : pegium::AstNode {};

struct ParentNode : pegium::AstNode {
  pointer<ChildNode> child;
};

} // namespace

TEST(NestTest, AssignsCurrentValueToFeature) {
  auto initParent = nest<&ParentNode::child>();
  pegium::RootCstNode dummyCst{pegium::text::TextSnapshot::copy("")}; pegium::AstArena arena{dummyCst};
  auto *current = arena.create<ChildNode>();

  EXPECT_EQ(initParent.getKind(), pegium::grammar::ElementKind::Nest);

  auto *created = initParent.getValue(current, arena);
  auto *parent = pegium::ast_ptr_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_NE(parent->child, nullptr);
  EXPECT_EQ(parent->child->getContainer(), parent);
}

TEST(NestTest, ExplicitFeatureTemplateAssignsCurrentValue) {
  auto initParent = nest<ParentNode, &ParentNode::child>();
  pegium::RootCstNode dummyCst{pegium::text::TextSnapshot::copy("")}; pegium::AstArena arena{dummyCst};
  auto *current = arena.create<ChildNode>();

  auto *created = initParent.getValue(current, arena);
  auto *parent = pegium::ast_ptr_cast<ParentNode>(created);

  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(parent->child != nullptr);
  EXPECT_NE(parent->child, nullptr);
  EXPECT_EQ(parent->child->getContainer(), parent);
}
