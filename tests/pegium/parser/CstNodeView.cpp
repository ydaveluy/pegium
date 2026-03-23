#include <gtest/gtest.h>

#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>

#include <ostream>

namespace {

struct DummyElement final : pegium::grammar::AbstractElement {
  explicit DummyElement(ElementKind kind) : kind(kind) {}

  constexpr ElementKind getKind() const noexcept override { return kind; }
  constexpr bool isNullable() const noexcept override { return false; }
  void print(std::ostream &os) const override { os << "dummy"; }

  ElementKind kind;
};

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

pegium::CstNodeView previous_sibling(const pegium::CstNodeView &node) {
  return node.previousSibling();
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

} // namespace

namespace pegium {
namespace {

TEST(CstNodeViewTest, NavigatesNodesAndSiblings) {
  DummyElement literal{grammar::ElementKind::Literal};
  DummyElement group{grammar::ElementKind::Group};

  auto harness = test::makeCstBuilderHarness("abc de");
  auto &builder = harness.builder;

  builder.leaf(0, 1, &literal);
  builder.enter();
  builder.leaf(1, 2, &literal);
  builder.leaf(2, 3, &literal);
  builder.exit(1, 3, &group);
  builder.leaf(4, 5, &literal);

  auto *root = builder.getRootCstNode();
  auto rootIt = root->begin();
  ASSERT_NE(rootIt, root->end());

  const auto first = *rootIt;
  const auto parent = *++rootIt;
  const auto trailing = *++rootIt;

  EXPECT_EQ(first.getText(), "a");
  EXPECT_EQ(parent.getText(), "bc");
  EXPECT_EQ(trailing.getText(), "d");

  EXPECT_EQ(first.next().getText(), "bc");
  EXPECT_EQ(parent.previous().getText(), "a");
  EXPECT_FALSE(first.previous().valid());

  EXPECT_EQ(first.nextSibling().getText(), "bc");
  EXPECT_EQ(parent.nextSibling().getText(), "d");
  EXPECT_FALSE(trailing.nextSibling().valid());

  auto childIt = parent.begin();
  ASSERT_NE(childIt, parent.end());
  const auto leftChild = *childIt;
  const auto rightChild = *++childIt;

  EXPECT_EQ(leftChild.getText(), "b");
  EXPECT_EQ(rightChild.getText(), "c");
  EXPECT_EQ(leftChild.nextSibling().getText(), "c");
  EXPECT_EQ(previous_sibling(parent).getText(), "a");
  EXPECT_EQ(previous_sibling(rightChild).getText(), "b");
  EXPECT_EQ(previous_sibling(trailing).getText(), "bc");
}

TEST(CstNodeViewTest, DetectsGapsBeforeAndAfterNodes) {
  DummyElement literal{grammar::ElementKind::Literal};

  auto harness = test::makeCstBuilderHarness(" a b ");
  auto &builder = harness.builder;

  builder.leaf(1, 2, &literal);
  builder.leaf(3, 4, &literal);

  auto *root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  const auto first = *it;
  const auto second = *++it;

  EXPECT_TRUE(first.hasGapBefore());
  EXPECT_TRUE(first.hasGapAfter());
  EXPECT_TRUE(second.hasGapBefore());
  EXPECT_TRUE(second.hasGapAfter());
}

TEST(CstNodeViewTest, LeafChildrenRangeIsEmpty) {
  DummyElement literal{grammar::ElementKind::Literal};

  auto harness = test::makeCstBuilderHarness("x");
  auto &builder = harness.builder;

  builder.leaf(0, 1, &literal);

  auto *root = builder.getRootCstNode();
  const auto leaf = *root->begin();

  EXPECT_EQ(leaf.begin(), leaf.end());

  std::size_t childCount = 0;
  for (const auto &child : leaf) {
    (void)child;
    ++childCount;
  }
  EXPECT_EQ(childCount, 0u);
}

TEST(CstNodeViewTest, IncludesZeroWidthChildAtParentEnd) {
  DummyElement literal{grammar::ElementKind::Literal};
  DummyElement group{grammar::ElementKind::Group};

  auto harness = test::makeCstBuilderHarness("x");
  auto &builder = harness.builder;

  builder.enter();
  builder.leaf(1, 1, &literal, false, true);
  builder.exit(0, 1, &group);

  auto *root = builder.getRootCstNode();
  auto rootIt = root->begin();
  ASSERT_NE(rootIt, root->end());
  const auto parent = *rootIt;

  auto childIt = parent.begin();
  ASSERT_NE(childIt, parent.end());
  const auto child = *childIt;

  EXPECT_EQ(child.getBegin(), 1u);
  EXPECT_EQ(child.getEnd(), 1u);
  EXPECT_TRUE(child.isRecovered());
  ++childIt;
  EXPECT_EQ(childIt, parent.end());
}

} // namespace
} // namespace pegium
