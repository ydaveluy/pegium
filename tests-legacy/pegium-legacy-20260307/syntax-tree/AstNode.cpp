#include <gtest/gtest.h>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/references/NameProvider.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/AstUtils.hpp>
#include <pegium/workspace/AstNodeDescriptionProvider.hpp>
#include <pegium/workspace/DefaultAstNodeDescriptionProvider.hpp>
#include <pegium/workspace/AstNodeLocator.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/references/DefaultNameProvider.hpp>

#include <memory>
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

struct RefParserNode : pegium::AstNode {
  reference<RefNode> single;
};

struct BranchNode : pegium::AstNode {
  pegium::AstNode *child = nullptr;
};

struct DerivedBranchNode : BranchNode {};

struct TreeRootNode : pegium::AstNode {
  BranchNode *single = nullptr;
  std::vector<BranchNode *> many;
};

struct DocLeafNode : pegium::AstNode {
  string name;
};

struct DocRootNode : pegium::AstNode {
  pointer<DocLeafNode> leaf;
};

template <typename RuleType>
std::unique_ptr<pegium::workspace::Document>
parse_rule(const RuleType &rule, std::string_view text,
           const pegium::parser::Skipper &skipper =
               pegium::parser::SkipperBuilder().build()) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  rule.parse(*document, skipper);
  return document;
}

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
  ASSERT_FALSE(owner.single.hasRefNode());
  ASSERT_FALSE(owner.many[0].hasRefNode());

  RefNode singleTarget;
  RefNode manyTarget;

  for (const auto &info : owner.getReferences()) {
    EXPECT_TRUE(info.isInstance(&singleTarget));
    EXPECT_FALSE(info.isInstance(static_cast<const pegium::AstNode *>(&owner)));
    EXPECT_FALSE(info.getRefText().empty());
    EXPECT_FALSE(info.getRefNode().has_value());

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

TEST(SyntaxTreeTest, ReferenceResolvesFromRefNodeTextWhenRefTextIsEmpty) {
  using namespace pegium::parser;

  ParserRule<RefParserNode> root{
      "Root", assign<&RefParserNode::single>("symbolName"_kw)};

  auto parsed = parse_rule(root, "symbolName", SkipperBuilder().build());
  ASSERT_TRUE(parsed->parseResult.value != nullptr);
  auto *owner = pegium::ast_ptr_cast<RefParserNode>(parsed->parseResult.value);
  ASSERT_TRUE(owner != nullptr);
  ASSERT_TRUE(owner->single.hasRefNode());
  EXPECT_EQ(owner->single.getRefText(), "symbolName");
  EXPECT_EQ(owner->single.getRefNode().getText(), "symbolName");

  owner->single.setRefText("");
  ASSERT_TRUE(owner->single.getRefText().empty());
  ASSERT_TRUE(owner->single.hasRefNode());

  RefNode target;
  std::string resolvedId;
  for (const auto &info : owner->getReferences()) {
    const auto refNode = info.getRefNode();
    ASSERT_TRUE(refNode.has_value());
    info.setRefNode(*refNode);
    EXPECT_TRUE(info.getRefText().empty());

    info.installResolver([&](const std::string &id) -> pegium::AstNode * {
      resolvedId = id;
      if (id == "symbolName") {
        return &target;
      }
      return nullptr;
    });
  }

  EXPECT_TRUE(resolvedId.empty());
  EXPECT_EQ(owner->single.get(), nullptr);
}

TEST(SyntaxTreeTest, ReferenceMoveKeepsRefTextRefNodeAndResolution) {
  using namespace pegium::parser;

  ParserRule<RefParserNode> root{
      "Root", assign<&RefParserNode::single>("movedName"_kw)};

  auto parsed = parse_rule(root, "movedName", SkipperBuilder().build());
  ASSERT_TRUE(parsed->parseResult.value != nullptr);
  auto *source = pegium::ast_ptr_cast<RefParserNode>(parsed->parseResult.value);
  ASSERT_TRUE(source != nullptr);
  ASSERT_TRUE(source->single.hasRefNode());

  RefOwnerNode movedOwner;
  movedOwner.single = std::move(source->single);
  movedOwner.addReference(&RefOwnerNode::single);

  EXPECT_EQ(movedOwner.single.getRefText(), "movedName");
  ASSERT_TRUE(movedOwner.single.hasRefNode());
  EXPECT_EQ(movedOwner.single.getRefNode().getText(), "movedName");

  RefNode target;
  for (const auto &info : movedOwner.getReferences()) {
    info.installResolver([&](const std::string &id) -> pegium::AstNode * {
      if (id == "movedName") {
        return &target;
      }
      return nullptr;
    });
  }

  EXPECT_EQ(movedOwner.single.get(), &target);
}

TEST(SyntaxTreeTest, DocumentIsResolvedFromRootCstNode) {
  using namespace pegium::parser;

  pegium::workspace::Document document;
  ParserRule<DocLeafNode> leafRule{"Leaf", assign<&DocLeafNode::name>("leaf"_kw)};
  ParserRule<DocRootNode> rootRule{"Root", assign<&DocRootNode::leaf>(leafRule)};

  document.setText("leaf");
  rootRule.parse(document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.cst != nullptr);
  ASSERT_TRUE(document.parseResult.value != nullptr);

  auto *root = pegium::ast_ptr_cast<DocRootNode>(document.parseResult.value);
  ASSERT_TRUE(root != nullptr);
  ASSERT_TRUE(root->leaf != nullptr);

  const auto *rootDocument = pegium::tryGetDocument(*root);
  ASSERT_NE(rootDocument, nullptr);
  EXPECT_EQ(pegium::tryGetDocument(*root->leaf), rootDocument);
  EXPECT_EQ(std::addressof(pegium::getDocument(*root->leaf)), rootDocument);
}

TEST(SyntaxTreeTest, AstNodeLocatorRoundTripsNestedPaths) {
  using namespace pegium::parser;

  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<DocLeafNode> leafRule{"Leaf",
                                   assign<&DocLeafNode::name>(tokenText)};
  ParserRule<DocRootNode> rootRule{"Root", assign<&DocRootNode::leaf>(leafRule)};

  auto parsed = parse_rule(rootRule, "child", SkipperBuilder().build());
  ASSERT_TRUE(parsed->parseResult.value != nullptr);

  auto *root = pegium::ast_ptr_cast<DocRootNode>(parsed->parseResult.value);
  ASSERT_TRUE(root != nullptr);
  ASSERT_TRUE(root->leaf != nullptr);

  const auto path = pegium::workspace::AstNodeLocator::getAstNodePath(*root->leaf);
  EXPECT_EQ(path, "/0");

  const auto *resolved =
      pegium::workspace::AstNodeLocator::getAstNode(*root, path);
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved, root->leaf.get());
}

TEST(SyntaxTreeTest, AstNodeDescriptionProviderBuildsPathBackedDescriptions) {
  using namespace pegium::parser;

  pegium::workspace::Document document;
  DataTypeRule<std::string> tokenText{"TokenText", some(w)};
  ParserRule<DocLeafNode> leafRule{"Leaf",
                                   assign<&DocLeafNode::name>(tokenText)};
  ParserRule<DocRootNode> rootRule{"Root", assign<&DocRootNode::leaf>(leafRule)};

  document.uri = "file:///ast-description.pg";
  document.setText("namedLeaf");
  rootRule.parse(document, SkipperBuilder().build());
  ASSERT_TRUE(document.parseResult.value != nullptr);
  ASSERT_TRUE(document.parseResult.cst != nullptr);

  auto *root = pegium::ast_ptr_cast<DocRootNode>(document.parseResult.value);
  ASSERT_TRUE(root != nullptr);
  ASSERT_TRUE(root->leaf != nullptr);

  pegium::services::SharedServices sharedServices;
  pegium::services::Services languageServices(sharedServices);
  languageServices.references.nameProvider =
      std::make_unique<pegium::references::DefaultNameProvider>();
  pegium::workspace::DefaultAstNodeDescriptionProvider descriptions(
      languageServices);
  auto leafDescription =
      descriptions.createDescription(*root->leaf, document);
  ASSERT_TRUE(leafDescription.has_value());
  EXPECT_EQ(leafDescription->name, "namedLeaf");
  EXPECT_EQ(leafDescription->documentUri, document.uri);
  EXPECT_EQ(leafDescription->path, "/0");

  const auto *resolved = pegium::workspace::AstNodeLocator::getAstNode(
      *root, leafDescription->path);
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved, root->leaf.get());
}
