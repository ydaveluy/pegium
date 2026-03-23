#include <gtest/gtest.h>

#include <pegium/core/parser/CstSearch.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/TestCstBuilderHarness.hpp>

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

TEST(CstSearchTest, FindFirstMatchingNodeSkipsHiddenAndSearchesDepthFirst) {
  DummyElement target{pegium::grammar::ElementKind::Literal};
  DummyElement other{pegium::grammar::ElementKind::CharacterRange};
  DummyElement group{pegium::grammar::ElementKind::Group};

  auto builderHarness = pegium::test::makeCstBuilderHarness("abcd");
  auto &builder = builderHarness.builder;

  builder.leaf(0, 1, &target, true);
  builder.enter();
  builder.leaf(1, 2, &other, false);
  builder.leaf(2, 3, &target, false);
  builder.exit(1, 3, &group);
  builder.leaf(3, 4, &target, false);

  auto root = builder.getRootCstNode();

  auto firstDeep = pegium::parser::detail::findFirstMatchingNode(*root, &target);
  ASSERT_TRUE(firstDeep.has_value());
  EXPECT_EQ(firstDeep->getText(), "c");

  auto firstRoot =
      pegium::parser::detail::findFirstRootMatchingNode(*root, &target);
  ASSERT_TRUE(firstRoot.has_value());
  EXPECT_EQ(firstRoot->getText(), "d");
}

TEST(CstSearchTest, FirstVisibleChildReturnsFirstVisibleOrNullopt) {
  DummyElement target{pegium::grammar::ElementKind::Literal};
  DummyElement group{pegium::grammar::ElementKind::Group};
  DummyElement missing{pegium::grammar::ElementKind::AnyCharacter};

  auto builderHarness = pegium::test::makeCstBuilderHarness("xy");
  auto &builder = builderHarness.builder;
  builder.enter();
  builder.leaf(0, 1, &target, true);
  builder.leaf(1, 2, &target, false);
  builder.exit(0, 2, &group);
  auto root = builder.getRootCstNode();

  auto it = root->begin();
  ASSERT_NE(it, root->end());
  auto parent = *it;

  auto firstVisible = pegium::parser::detail::firstVisibleChild(parent);
  ASSERT_TRUE(firstVisible.has_value());
  EXPECT_EQ(firstVisible->getText(), "y");

  auto foundInParent =
      pegium::parser::detail::findFirstMatchingNode(parent, &target);
  ASSERT_TRUE(foundInParent.has_value());
  EXPECT_EQ(foundInParent->getText(), "y");

  auto notFound = pegium::parser::detail::findFirstMatchingNode(parent, &missing);
  EXPECT_FALSE(notFound.has_value());

  auto hiddenOnlyBuilderHarness = pegium::test::makeCstBuilderHarness("z");
  auto &hiddenOnlyBuilder = hiddenOnlyBuilderHarness.builder;
  hiddenOnlyBuilder.enter();
  hiddenOnlyBuilder.leaf(0, 1, &target, true);
  hiddenOnlyBuilder.exit(0, 1, &group);
  auto hiddenOnlyRoot = hiddenOnlyBuilder.getRootCstNode();
  auto hiddenParent = *hiddenOnlyRoot->begin();

  auto noneVisible = pegium::parser::detail::firstVisibleChild(hiddenParent);
  EXPECT_FALSE(noneVisible.has_value());
}

TEST(CstSearchTest, RootLevelSearchReturnsNulloptWhenNoVisibleMatchExists) {
  DummyElement target{pegium::grammar::ElementKind::Literal};
  DummyElement other{pegium::grammar::ElementKind::CharacterRange};

  auto builderHarness = pegium::test::makeCstBuilderHarness("ab");
  auto &builder = builderHarness.builder;
  builder.leaf(0, 1, &other, false);
  builder.leaf(1, 2, &target, true);
  auto root = builder.getRootCstNode();

  auto rootFound =
      pegium::parser::detail::findFirstRootMatchingNode(*root, &target);
  EXPECT_FALSE(rootFound.has_value());

  auto anyFound = pegium::parser::detail::findFirstMatchingNode(*root, &target);
  EXPECT_FALSE(anyFound.has_value());
}
