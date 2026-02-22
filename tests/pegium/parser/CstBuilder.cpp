#include <gtest/gtest.h>

#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

#include <ostream>
#include <vector>

namespace {

struct DummyGrammarElement final : pegium::grammar::AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  void print(std::ostream &os) const override { os << "dummy"; }
};

} // namespace

TEST(SyntaxTreeTest, BuilderRewindRestoresCheckpoint) {
  pegium::CstBuilder builder{"xyz"};
  const char *begin = builder.input_begin();
  const auto checkpoint = builder.mark(begin);

  builder.enter(begin);
  builder.leaf(begin, begin + 1, nullptr, false);
  builder.exit(begin + 1, nullptr);
  ASSERT_GT(builder.node_count(), 0u);

  const char *rewound = builder.rewind(checkpoint);
  EXPECT_EQ(rewound, begin);
  EXPECT_EQ(builder.node_count(), 0u);

  auto root = builder.finalize();
  std::size_t count = 0;
  for (const auto &node : *root) {
    (void)node;
    ++count;
  }
  EXPECT_EQ(count, 0u);
}

TEST(SyntaxTreeTest, BuilderResetClearsStateAndAllowsReuse) {
  pegium::CstBuilder builder{"xy"};
  const char *begin = builder.input_begin();

  builder.leaf(begin, begin + 1, nullptr, false);
  auto firstRoot = builder.finalize();

  std::vector<pegium::CstNodeView> firstNodes;
  for (const auto &node : *firstRoot) {
    firstNodes.push_back(node);
  }
  ASSERT_EQ(firstNodes.size(), 1u);
  EXPECT_EQ(firstNodes[0].getText(), "x");

  builder.reset();
  EXPECT_EQ(builder.node_count(), 0u);

  builder.leaf(begin + 1, begin + 2, nullptr, false);
  auto secondRoot = builder.finalize();

  std::vector<pegium::CstNodeView> secondNodes;
  for (const auto &node : *secondRoot) {
    secondNodes.push_back(node);
  }
  ASSERT_EQ(secondNodes.size(), 1u);
  EXPECT_EQ(secondNodes[0].getText(), "y");
}

TEST(SyntaxTreeTest, BuilderOverrideGrammarElementUpdatesNode) {
  static const DummyGrammarElement grammarElement{};
  pegium::CstBuilder builder{"a"};
  const char *begin = builder.input_begin();

  builder.leaf(begin, begin + 1, nullptr, false);
  builder.override_grammar_element(0u, &grammarElement);
  auto root = builder.finalize();

  std::vector<pegium::CstNodeView> topLevelNodes;
  for (const auto &node : *root) {
    topLevelNodes.push_back(node);
  }
  ASSERT_EQ(topLevelNodes.size(), 1u);
  EXPECT_EQ(topLevelNodes.front().getGrammarElement(), &grammarElement);
}

TEST(SyntaxTreeTest, BuilderFinalizeIsIdempotentAndLeafHasNoChildren) {
  pegium::CstBuilder builder{"a"};
  const char *begin = builder.input_begin();
  builder.leaf(begin, begin + 1, nullptr, false);

  auto root1 = builder.finalize();
  auto root2 = builder.finalize();

  EXPECT_EQ(root1.get(), root2.get());

  auto it = root1->begin();
  ASSERT_NE(it, root1->end());
  auto leaf = *it;
  EXPECT_TRUE(leaf.isLeaf());
  EXPECT_EQ(leaf.begin(), leaf.end());
}

TEST(SyntaxTreeTest, EmptyRootHasNoTopLevelNodes) {
  pegium::RootCstNode root{""};
  EXPECT_EQ(root.begin(), root.end());
}

TEST(SyntaxTreeTest, NodeViewExposesOffsetsAndText) {
  pegium::CstBuilder builder{"abc"};
  const char *begin = builder.input_begin();
  builder.leaf(begin + 1, begin + 3, nullptr, false);

  auto root = builder.finalize();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  auto node = *it;

  EXPECT_EQ(node.getBegin(), 1u);
  EXPECT_EQ(node.getEnd(), 3u);
  EXPECT_EQ(node.getText(), "bc");
  EXPECT_FALSE(node.isHidden());
}
