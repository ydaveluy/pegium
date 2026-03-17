#include <gtest/gtest.h>

#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/syntax-tree/CstUtils.hpp>

#include <ostream>

namespace {

struct DummyElement final : pegium::grammar::AbstractElement {
  explicit DummyElement(ElementKind kind) : kind(kind) {}

  constexpr ElementKind getKind() const noexcept override { return kind; }
  constexpr bool isNullable() const noexcept override { return false; }
  void print(std::ostream &os) const override { os << "dummy"; }

  ElementKind kind;
};

} // namespace

namespace pegium {
namespace {

TEST(CstUtilsTest, FindNodeAtOffsetDescendsToDeepestVisibleNode) {
  DummyElement literal{grammar::ElementKind::Literal};
  DummyElement group{grammar::ElementKind::Group};

  auto harness = test::makeCstBuilderHarness("abc");
  auto &builder = harness.builder;

  builder.enter();
  builder.leaf(0, 1, &literal, true);
  builder.enter();
  builder.leaf(1, 2, &literal, false);
  builder.exit(1, 2, &group);
  builder.leaf(2, 3, &literal, false);
  builder.exit(0, 3, &group);

  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);

  const auto nested = find_node_at_offset(*root, 1);
  ASSERT_TRUE(nested.has_value());
  EXPECT_EQ(nested->getText(), "b");
}

TEST(CstUtilsTest, FindNodeAtOffsetKeepsLeftBoundaryPreference) {
  DummyElement literal{grammar::ElementKind::Literal};
  DummyElement group{grammar::ElementKind::Group};

  auto harness = test::makeCstBuilderHarness("ab");
  auto &builder = harness.builder;

  builder.enter();
  builder.leaf(0, 1, &literal);
  builder.leaf(1, 2, &literal);
  builder.exit(0, 2, &group);

  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);

  const auto atBoundary = find_node_at_offset(*root, 1);
  ASSERT_TRUE(atBoundary.has_value());
  EXPECT_EQ(atBoundary->getText(), "a");
}

TEST(CstUtilsTest, GetInteriorNodesDescendsIntoSharedContainer) {
  DummyElement literal{grammar::ElementKind::Literal};
  DummyElement group{grammar::ElementKind::Group};

  auto harness = test::makeCstBuilderHarness("abcde");
  auto &builder = harness.builder;

  builder.enter();
  builder.leaf(0, 1, &literal);
  builder.enter();
  builder.leaf(1, 2, &literal);
  builder.leaf(2, 3, &literal);
  builder.leaf(3, 4, &literal);
  builder.exit(1, 4, &group);
  builder.leaf(4, 5, &literal);
  builder.exit(0, 5, &group);

  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);

  const auto outer = root->get(0);
  ASSERT_TRUE(outer.valid());
  auto outerChildren = outer.begin();
  ASSERT_NE(outerChildren, outer.end());
  const auto first = *outerChildren;
  const auto inner = *++outerChildren;
  const auto trailing = *++outerChildren;
  ASSERT_TRUE(first.valid());
  ASSERT_TRUE(inner.valid());
  ASSERT_TRUE(trailing.valid());

  auto innerChildren = inner.begin();
  ASSERT_NE(innerChildren, inner.end());
  const auto left = *innerChildren;
  const auto middle = *++innerChildren;
  const auto right = *++innerChildren;
  ASSERT_TRUE(left.valid());
  ASSERT_TRUE(middle.valid());
  ASSERT_TRUE(right.valid());

  const auto interior = get_interior_nodes(left, right);
  ASSERT_EQ(interior.size(), 1u);
  EXPECT_EQ(interior.front().getText(), "c");
}

} // namespace
} // namespace pegium
