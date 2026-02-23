#include <gtest/gtest.h>

#include <pegium/parser/CstSearch.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>

#include <ostream>

namespace {

struct DummyElement final : pegium::grammar::AbstractElement {
  explicit DummyElement(ElementKind kind) : kind(kind) {}

  constexpr ElementKind getKind() const noexcept override { return kind; }
  void print(std::ostream &os) const override { os << "dummy"; }

  ElementKind kind;
};

} // namespace

TEST(CstSearchTest, FindFirstMatchingNodeSkipsHiddenAndSearchesDepthFirst) {
  DummyElement target{pegium::grammar::ElementKind::Literal};
  DummyElement other{pegium::grammar::ElementKind::CharacterRange};
  DummyElement group{pegium::grammar::ElementKind::Group};

  pegium::CstBuilder builder{"abcd"};
  const char *begin = builder.input_begin();

  builder.leaf(begin, begin + 1, &target, true);
  builder.enter();
  builder.leaf(begin + 1, begin + 2, &other, false);
  builder.leaf(begin + 2, begin + 3, &target, false);
  builder.exit(begin + 1, begin + 3, &group);
  builder.leaf(begin + 3, begin + 4, &target, false);

  auto root = builder.finalize();

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

  pegium::CstBuilder builder{"xy"};
  const char *begin = builder.input_begin();
  builder.enter();
  builder.leaf(begin, begin + 1, &target, true);
  builder.leaf(begin + 1, begin + 2, &target, false);
  builder.exit(begin, begin + 2, &group);
  auto root = builder.finalize();

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

  pegium::CstBuilder hiddenOnlyBuilder{"z"};
  const char *b = hiddenOnlyBuilder.input_begin();
  hiddenOnlyBuilder.enter();
  hiddenOnlyBuilder.leaf(b, b + 1, &target, true);
  hiddenOnlyBuilder.exit(b, b + 1, &group);
  auto hiddenOnlyRoot = hiddenOnlyBuilder.finalize();
  auto hiddenParent = *hiddenOnlyRoot->begin();

  auto noneVisible = pegium::parser::detail::firstVisibleChild(hiddenParent);
  EXPECT_FALSE(noneVisible.has_value());
}

TEST(CstSearchTest, RootLevelSearchReturnsNulloptWhenNoVisibleMatchExists) {
  DummyElement target{pegium::grammar::ElementKind::Literal};
  DummyElement other{pegium::grammar::ElementKind::CharacterRange};

  pegium::CstBuilder builder{"ab"};
  const char *begin = builder.input_begin();
  builder.leaf(begin, begin + 1, &other, false);
  builder.leaf(begin + 1, begin + 2, &target, true);
  auto root = builder.finalize();

  auto rootFound =
      pegium::parser::detail::findFirstRootMatchingNode(*root, &target);
  EXPECT_FALSE(rootFound.has_value());

  auto anyFound = pegium::parser::detail::findFirstMatchingNode(*root, &target);
  EXPECT_FALSE(anyFound.has_value());
}
