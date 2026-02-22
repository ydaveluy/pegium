#include <gtest/gtest.h>

#include <pegium/syntax-tree/AstNode.hpp>

#include <string>
#include <vector>

namespace {

struct ChildNode;

struct ParentNode : pegium::AstNode {
  ChildNode *single = nullptr;
  std::vector<ChildNode *> children;
};

struct ChildNode : pegium::AstNode {};

struct RefNode : pegium::AstNode {};

struct RefOwnerNode : pegium::AstNode {
  reference<RefNode> single;
  vector<reference<RefNode>> many;
};

struct BranchNode : pegium::AstNode {
  pegium::AstNode *child = nullptr;
};

struct DerivedBranchNode : BranchNode {};

struct TreeRootNode : pegium::AstNode {
  BranchNode *single = nullptr;
  std::vector<BranchNode *> many;
};

} // namespace

TEST(SyntaxTreeTest, AstNodeSetContainerReparentsNode) {
  ParentNode firstParent;
  ParentNode secondParent;
  ChildNode child;

  child.setContainer(&firstParent, &ParentNode::single);
  EXPECT_EQ(child.getContainer(), &firstParent);
  EXPECT_EQ(static_cast<const ChildNode &>(child).getContainer(), &firstParent);

  std::size_t firstCount = 0;
  for (const auto *node : firstParent.getContent()) {
    if (node == &child) {
      ++firstCount;
    }
  }
  EXPECT_EQ(firstCount, 1u);

  child.setContainer(&secondParent, &ParentNode::children, 1u);
  EXPECT_EQ(child.getContainer(), &secondParent);

  std::size_t oldParentCount = 0;
  for (const auto *node : firstParent.getContent()) {
    if (node == &child) {
      ++oldParentCount;
    }
  }
  EXPECT_EQ(oldParentCount, 0u);

  std::size_t newParentCount = 0;
  for (const auto *node : secondParent.getContent()) {
    if (node == &child) {
      ++newParentCount;
    }
  }
  EXPECT_EQ(newParentCount, 1u);
}

TEST(SyntaxTreeTest, AstNodeTraversalAndTypedContainerQueriesWork) {
  TreeRootNode root;
  BranchNode first;
  DerivedBranchNode second;
  BranchNode nested;

  first.setContainer(&root, &TreeRootNode::single);
  second.setContainer(&root, &TreeRootNode::many, 0u);
  nested.setContainer(&first, &BranchNode::child);

  std::size_t directCount = 0;
  for (auto *node : root.getContent()) {
    (void)node;
    ++directCount;
  }
  EXPECT_EQ(directCount, 2u);

  const TreeRootNode &constRoot = root;
  std::size_t constDirectCount = 0;
  for (const auto *node : constRoot.getContent()) {
    (void)node;
    ++constDirectCount;
  }
  EXPECT_EQ(constDirectCount, 2u);

  std::size_t branchCount = 0;
  for (auto *node : root.getContent<BranchNode>()) {
    (void)node;
    ++branchCount;
  }
  EXPECT_EQ(branchCount, 2u);

  std::size_t derivedBranchCount = 0;
  for (auto *node : root.getContent<DerivedBranchNode>()) {
    (void)node;
    ++derivedBranchCount;
  }
  EXPECT_EQ(derivedBranchCount, 1u);

  std::size_t allCount = 0;
  for (auto *node : root.getAllContent()) {
    (void)node;
    ++allCount;
  }
  EXPECT_EQ(allCount, 3u);

  std::size_t constAllCount = 0;
  for (const auto *node : constRoot.getAllContent()) {
    (void)node;
    ++constAllCount;
  }
  EXPECT_EQ(constAllCount, 3u);

  EXPECT_EQ(nested.getContainer<BranchNode>(), &nested);
  EXPECT_EQ(nested.getContainer<TreeRootNode>(), &root);
  const BranchNode &constNested = nested;
  EXPECT_EQ(constNested.getContainer<TreeRootNode>(), &root);
}

TEST(SyntaxTreeTest, TypedContainerQueriesReturnNullptrWhenNoAncestorMatches) {
  BranchNode orphan;
  EXPECT_EQ(orphan.getContainer<TreeRootNode>(), nullptr);

  const BranchNode &constOrphan = orphan;
  EXPECT_EQ(constOrphan.getContainer<TreeRootNode>(), nullptr);
}

TEST(SyntaxTreeTest, StandaloneAstNodeHasNoContainer) {
  pegium::AstNode node;
  EXPECT_EQ(node.getContainer(), nullptr);
  EXPECT_EQ(node.getContainer<BranchNode>(), nullptr);

  const pegium::AstNode &constNode = node;
  EXPECT_EQ(constNode.getContainer(), nullptr);
  EXPECT_EQ(constNode.getContainer<BranchNode>(), nullptr);
}

TEST(SyntaxTreeTest, ReferenceInfoInstallsResolverAndResolvesReferences) {
  RefOwnerNode owner;
  owner.single = pegium::Reference<RefNode>{"single"};
  owner.many.emplace_back(pegium::Reference<RefNode>{"many"});
  owner.addReference(&RefOwnerNode::single);
  owner.addReference(&RefOwnerNode::many, 0u);

  RefNode singleTarget;
  RefNode manyTarget;

  for (const auto &info : owner.getReferences()) {
    EXPECT_TRUE(info.isInstance(&singleTarget));
    EXPECT_FALSE(info.isInstance(static_cast<const pegium::AstNode *>(&owner)));

    info.installResolver([&](const std::string &id) -> pegium::AstNode * {
      if (id == "single") {
        return &singleTarget;
      }
      if (id == "many") {
        return &manyTarget;
      }
      return nullptr;
    });
  }

  EXPECT_EQ(owner.single.get(), &singleTarget);
  EXPECT_EQ(owner.many[0].get(), &manyTarget);
  EXPECT_TRUE(static_cast<bool>(owner.single));
  EXPECT_EQ(owner.single->getContainer(), nullptr);
}
