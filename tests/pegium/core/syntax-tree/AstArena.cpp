#include <gtest/gtest.h>

#include <pegium/core/syntax-tree/AstArena.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

#include <string>
#include <vector>

namespace pegium {
namespace {

inline RootCstNode make_dummy_cst() {
  return RootCstNode(text::TextSnapshot::copy(""));
}

struct CountingNode : AstNode {
  static int liveCount;
  static std::vector<int> destructionOrder;

  int id = 0;

  CountingNode() noexcept { ++liveCount; }
  ~CountingNode() noexcept override {
    destructionOrder.push_back(id);
    --liveCount;
  }
};
int CountingNode::liveCount = 0;
std::vector<int> CountingNode::destructionOrder;

struct StringHolderNode : AstNode {
  std::string payload;
};

struct VectorHolderNode : AstNode {
  std::vector<int> values;
};

struct DerivedNamedNode : NamedAstNode {
  int extra = 0;
};

class AstArenaTest : public ::testing::Test {
protected:
  void SetUp() override {
    CountingNode::liveCount = 0;
    CountingNode::destructionOrder.clear();
  }
};

TEST_F(AstArenaTest, EmptyArenaHasNoNodes) {
  auto cst = make_dummy_cst(); AstArena arena(cst);
  EXPECT_TRUE(arena.empty());
  EXPECT_EQ(arena.size(), 0u);
}

TEST_F(AstArenaTest, CreatesSingleNodeAndDestroysIt) {
  {
    auto cst = make_dummy_cst(); AstArena arena(cst);
    auto *node = arena.create<CountingNode>();
    ASSERT_NE(node, nullptr);
    node->id = 1;
    EXPECT_EQ(arena.size(), 1u);
    EXPECT_FALSE(arena.empty());
    EXPECT_EQ(CountingNode::liveCount, 1);
  }
  EXPECT_EQ(CountingNode::liveCount, 0);
  ASSERT_EQ(CountingNode::destructionOrder.size(), 1u);
  EXPECT_EQ(CountingNode::destructionOrder[0], 1);
}

TEST_F(AstArenaTest, DestroysNodesInReverseConstructionOrder) {
  {
    auto cst = make_dummy_cst(); AstArena arena(cst);
    for (int i = 0; i < 5; ++i) {
      auto *node = arena.create<CountingNode>();
      node->id = i;
    }
    EXPECT_EQ(CountingNode::liveCount, 5);
  }
  EXPECT_EQ(CountingNode::liveCount, 0);
  ASSERT_EQ(CountingNode::destructionOrder.size(), 5u);
  // LIFO: 4, 3, 2, 1, 0
  EXPECT_EQ(CountingNode::destructionOrder[0], 4);
  EXPECT_EQ(CountingNode::destructionOrder[1], 3);
  EXPECT_EQ(CountingNode::destructionOrder[2], 2);
  EXPECT_EQ(CountingNode::destructionOrder[3], 1);
  EXPECT_EQ(CountingNode::destructionOrder[4], 0);
}

TEST_F(AstArenaTest, DestroysNonTrivialFields) {
  // No leak detection here, but ASan would catch a missed destructor.
  auto cst = make_dummy_cst(); AstArena arena(cst);
  auto *strNode = arena.create<StringHolderNode>();
  strNode->payload = std::string(1024, 'x');  // forces heap allocation in std::string

  auto *vecNode = arena.create<VectorHolderNode>();
  vecNode->values.assign(256, 42);

  EXPECT_EQ(strNode->payload.size(), 1024u);
  EXPECT_EQ(vecNode->values.size(), 256u);
  // Arena destruction calls ~StringHolderNode and ~VectorHolderNode,
  // which in turn release the heap-allocated string/vector buffers.
}

TEST_F(AstArenaTest, ScalesAcrossChunkBoundary) {
  constexpr std::uint32_t kChildren = 5000;  // > chunk_size (4096)
  auto cst = make_dummy_cst(); AstArena arena(cst);
  for (std::uint32_t i = 0; i < kChildren; ++i) {
    auto *node = arena.create<CountingNode>();
    node->id = static_cast<int>(i);
  }
  EXPECT_EQ(arena.size(), kChildren);
  EXPECT_EQ(CountingNode::liveCount, static_cast<int>(kChildren));
}

TEST_F(AstArenaTest, SupportsDerivedTypes) {
  auto cst = make_dummy_cst(); AstArena arena(cst);
  auto *named = arena.create<DerivedNamedNode>();
  ASSERT_NE(named, nullptr);
  named->name = "alpha";
  named->extra = 7;

  // dynamic_cast still works on arena-allocated nodes.
  AstNode *base = named;
  EXPECT_NE(dynamic_cast<NamedAstNode *>(base), nullptr);
  EXPECT_NE(dynamic_cast<DerivedNamedNode *>(base), nullptr);
}

TEST_F(AstArenaTest, NodesPreserveTreeStructure) {
  auto cst = make_dummy_cst(); AstArena arena(cst);
  auto *root = arena.create<CountingNode>();
  auto *child1 = arena.create<CountingNode>();
  auto *child2 = arena.create<CountingNode>();
  child1->setContainer(*root);
  child2->setContainer(*root);

  // Iterate via getContent (linked-list AstNode children).
  std::vector<AstNode *> seen;
  for (AstNode *child : root->getContent()) {
    seen.push_back(child);
  }
  ASSERT_EQ(seen.size(), 2u);
  EXPECT_EQ(seen[0], child1);
  EXPECT_EQ(seen[1], child2);
}

} // namespace
} // namespace pegium
