#include <gtest/gtest.h>

#include <stdexcept>

#include <pegium/core/workspace/DefaultAstNodeLocator.hpp>

namespace pegium::workspace {
namespace {

struct LocatorLeaf final : AstNode {};

struct LocatorBranch final : AstNode {
  std::unique_ptr<LocatorLeaf> left;
  std::unique_ptr<LocatorLeaf> right;
};

struct LocatorRoot final : AstNode {
  std::unique_ptr<LocatorBranch> branch;
  std::vector<std::unique_ptr<LocatorLeaf>> leaves;
};

TEST(AstNodeLocatorTest, ComputesAndResolvesPropertyPaths) {
  DefaultAstNodeLocator locator;

  LocatorRoot root;
  root.branch = std::make_unique<LocatorBranch>();
  root.leaves.push_back(std::make_unique<LocatorLeaf>());
  root.leaves.push_back(std::make_unique<LocatorLeaf>());
  root.branch->left = std::make_unique<LocatorLeaf>();
  root.branch->right = std::make_unique<LocatorLeaf>();

  root.branch->setContainer<LocatorRoot, &LocatorRoot::branch>(root);
  root.leaves[0]->setContainer<LocatorRoot, &LocatorRoot::leaves>(root, 0);
  root.leaves[1]->setContainer<LocatorRoot, &LocatorRoot::leaves>(root, 1);
  root.branch->left->setContainer<LocatorBranch, &LocatorBranch::left>(
      *root.branch);
  root.branch->right->setContainer<LocatorBranch, &LocatorBranch::right>(
      *root.branch);

  EXPECT_EQ(locator.getAstNodePath(root), "");
  EXPECT_EQ(locator.getAstNodePath(*root.branch), "/branch");
  EXPECT_EQ(locator.getAstNodePath(*root.branch->right), "/branch/right");
  EXPECT_EQ(locator.getAstNodePath(*root.leaves[1]), "/leaves@1");

  EXPECT_EQ(locator.getAstNode(root, "/branch"), root.branch.get());
  EXPECT_EQ(locator.getAstNode(root, "/branch/right"),
            root.branch->right.get());
  EXPECT_EQ(locator.getAstNode(root, "/leaves@1"), root.leaves[1].get());
  EXPECT_EQ(locator.getAstNode(root, "/3"), nullptr);
  EXPECT_EQ(locator.getAstNode(root, "/branch/x"), nullptr);
  EXPECT_EQ(locator.getAstNode(root, "/branch/"), root.branch.get());
}

TEST(AstNodeLocatorTest, UpdatesPropertyPathsWhenNodeIsReparented) {
  DefaultAstNodeLocator locator;

  LocatorRoot root;
  root.branch = std::make_unique<LocatorBranch>();
  root.leaves.push_back(std::make_unique<LocatorLeaf>());
  root.leaves.push_back(std::make_unique<LocatorLeaf>());

  root.branch->setContainer<LocatorRoot, &LocatorRoot::branch>(root);
  root.leaves[0]->setContainer<LocatorRoot, &LocatorRoot::leaves>(root, 0);
  root.leaves[1]->setContainer<LocatorRoot, &LocatorRoot::leaves>(root, 1);

  root.branch->left = std::move(root.leaves[0]);
  root.leaves.erase(root.leaves.begin());
  root.branch->left->setContainer<LocatorBranch, &LocatorBranch::left>(
      *root.branch);

  EXPECT_EQ(locator.getAstNodePath(*root.branch->left), "/branch/left");
  EXPECT_EQ(locator.getAstNodePath(*root.leaves[0]), "/leaves@1");
  EXPECT_EQ(locator.getAstNode(root, "/branch/left"),
            root.branch->left.get());
  EXPECT_EQ(locator.getAstNode(root, "/leaves@1"), root.leaves[0].get());
}

TEST(AstNodeLocatorTest, ThrowsWithoutPropertyMetadata) {
  DefaultAstNodeLocator locator;

  LocatorRoot root;
  root.branch = std::make_unique<LocatorBranch>();
  root.leaves.push_back(std::make_unique<LocatorLeaf>());

  root.branch->attachToContainer(root, {});
  root.leaves[0]->attachToContainer(root, {}, 0);

  EXPECT_THROW((void)locator.getAstNodePath(*root.branch), std::invalid_argument);
  EXPECT_THROW((void)locator.getAstNodePath(*root.leaves[0]),
               std::invalid_argument);
  EXPECT_EQ(locator.getAstNode(root, "/branch"), nullptr);
  EXPECT_EQ(locator.getAstNode(root, "/leaves@0"), nullptr);
}

} // namespace
} // namespace pegium::workspace
