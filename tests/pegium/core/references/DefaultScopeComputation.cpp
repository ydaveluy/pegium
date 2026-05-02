#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <memory>
#include <string>
#include <typeindex>
#include <utility>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/references/DefaultScopeComputation.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/references/NameProvider.hpp>
#include <pegium/core/workspace/AstNodeDescriptionProvider.hpp>

namespace pegium::references {
namespace {

struct NamedScopeNode : AstNode {
  explicit NamedScopeNode(std::string value = {}) : name(std::move(value)) {}
  std::string name;
};

struct ScopeLeaf final : NamedScopeNode {
  using NamedScopeNode::NamedScopeNode;
};

struct ScopeBranch final : NamedScopeNode {
  using NamedScopeNode::NamedScopeNode;
  ScopeLeaf *directLeaf = nullptr;
  std::vector<ScopeLeaf *> leaves;
};

struct ScopeRoot final : NamedScopeNode {
  using NamedScopeNode::NamedScopeNode;
  ScopeBranch *branch = nullptr;
  std::vector<ScopeLeaf *> leaves;
};

class TestNameProvider final : public NameProvider {
public:
  [[nodiscard]] AstNodeName nameOf(const AstNode &node) const override {
    if (const auto *named = dynamic_cast<const NamedScopeNode *>(&node);
        named != nullptr && !named->name.empty()) {
      return {named->name, {}};
    }
    return {};
  }
};

class PrefixNameProvider final : public NameProvider {
public:
  explicit PrefixNameProvider(std::string prefix) : _prefix(std::move(prefix)) {}

  [[nodiscard]] AstNodeName nameOf(const AstNode &node) const override {
    if (const auto *named = dynamic_cast<const NamedScopeNode *>(&node);
        named != nullptr && !named->name.empty()) {
      return {_prefix + named->name, {}};
    }
    return {};
  }

private:
  std::string _prefix;
};

class TestDescriptionProvider final : public workspace::AstNodeDescriptionProvider {
public:
  explicit TestDescriptionProvider(std::string prefix = {})
      : _prefix(std::move(prefix)) {}

  [[nodiscard]] std::optional<workspace::AstNodeDescription>
  createDescription(const AstNode &node, const workspace::Document &document,
                    AstNodeName nameInfo) const override {
    if (nameInfo.empty()) {
      return std::nullopt;
    }
    auto fullName = _prefix + nameInfo.name;

    auto symbolId =
        static_cast<workspace::SymbolId>(reinterpret_cast<std::uintptr_t>(&node));
    if (symbolId == workspace::InvalidSymbolId) {
      symbolId = 0;
    }

    return workspace::AstNodeDescription{
        .name = std::move(fullName),
        .type = std::type_index(typeid(node)),
        .documentId = document.id,
        .symbolId = symbolId,
        .nameLength = static_cast<TextOffset>(_prefix.size() + nameInfo.name.size()),
    };
  }

private:
  std::string _prefix;
};

std::shared_ptr<workspace::Document> make_scope_document() {
  auto document = std::make_shared<workspace::Document>(
      test::make_text_document(test::make_file_uri("scope-computation.test"),
                               "test", ""));
  document->id = 7;

  document->parseResult.cst = std::make_unique<RootCstNode>(
      pegium::text::TextSnapshot::copy(""));
  document->parseResult.astArena =
      std::make_unique<pegium::AstArena>(*document->parseResult.cst);
  auto &arena = *document->parseResult.astArena;
  arena.attachDocument(*document);
  auto *root = arena.create<ScopeRoot>("root");
  root->branch = arena.create<ScopeBranch>("branch");
  root->leaves.push_back(arena.create<ScopeLeaf>("leaf"));
  root->leaves.push_back(arena.create<ScopeLeaf>());
  root->branch->directLeaf = arena.create<ScopeLeaf>("nested");
  root->branch->leaves.push_back(arena.create<ScopeLeaf>("nested2"));

  root->branch->setContainer(*root);
  root->leaves[0]->setContainer(*root);
  root->leaves[1]->setContainer(*root);
  root->branch->directLeaf->setContainer(*root->branch);
  root->branch->leaves[0]->setContainer(*root->branch);

  document->parseResult.value = root;
  return document;
}

std::vector<std::string>
collect_names(const std::vector<workspace::AstNodeDescription> &descriptions) {
  std::vector<std::string> names;
  names.reserve(descriptions.size());
  for (const auto &description : descriptions) {
    names.push_back(description.name);
  }
  std::ranges::sort(names);
  return names;
}

std::vector<std::string> collect_local_names(const workspace::LocalSymbols &symbols) {
  std::vector<std::string> names;
  names.reserve(symbols.size());
  for (const auto &[container, entries] : symbols) {
    (void)container;
    for (const auto &bucket : entries.buckets) {
      for (const auto &description : bucket.ownedEntries) {
        names.push_back(description.name);
      }
    }
  }
  std::ranges::sort(names);
  return names;
}

TEST(DefaultScopeComputationTest, ExportsRootAndDirectChildrenOnlyByDefault) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  services->references.nameProvider = std::make_unique<TestNameProvider>();
  services->workspace.astNodeDescriptionProvider =
      std::make_unique<TestDescriptionProvider>();

  const auto document = make_scope_document();
  const auto exports =
      services->references.scopeComputation->collectExportedSymbols(*document, {});

  EXPECT_EQ(collect_names(exports),
            (std::vector<std::string>{"branch", "leaf", "root"}));
}

TEST(DefaultScopeComputationTest, CollectsNamedDescendantsIntoLocalSymbols) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  services->references.nameProvider = std::make_unique<TestNameProvider>();
  services->workspace.astNodeDescriptionProvider =
      std::make_unique<TestDescriptionProvider>();

  const auto document = make_scope_document();
  const auto symbols =
      services->references.scopeComputation->collectLocalSymbols(*document, {});

  EXPECT_EQ(collect_local_names(symbols),
            (std::vector<std::string>{"branch", "leaf", "nested", "nested2"}));
}

TEST(DefaultScopeComputationTest, UsesProvidersFromCoreServicesAtCallTime) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  auto *scopeComputation = services->references.scopeComputation.get();

  services->references.nameProvider =
      std::make_unique<PrefixNameProvider>("np:");
  services->workspace.astNodeDescriptionProvider =
      std::make_unique<TestDescriptionProvider>("dp:");

  const auto document = make_scope_document();
  const auto exports = scopeComputation->collectExportedSymbols(*document, {});

  EXPECT_EQ(collect_names(exports),
            (std::vector<std::string>{"dp:np:branch", "dp:np:leaf",
                                      "dp:np:root"}));
}

TEST(DefaultScopeComputationTest, PropagatesCancellationInExportsAndLocals) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  services->references.nameProvider = std::make_unique<TestNameProvider>();
  services->workspace.astNodeDescriptionProvider =
      std::make_unique<TestDescriptionProvider>();

  const auto document = make_scope_document();
  utils::CancellationTokenSource source;
  source.request_stop();

  EXPECT_THROW((void)services->references.scopeComputation->collectExportedSymbols(
                   *document, source.get_token()),
               utils::OperationCancelled);
  EXPECT_THROW((void)services->references.scopeComputation->collectLocalSymbols(
                   *document, source.get_token()),
               utils::OperationCancelled);
}

TEST(DefaultScopeComputationTest, ReturnsEmptyWithoutAst) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  services->references.nameProvider = std::make_unique<TestNameProvider>();
  services->workspace.astNodeDescriptionProvider =
      std::make_unique<TestDescriptionProvider>();

  workspace::Document document(
      test::make_text_document(test::make_file_uri("no-ast.test"), "test", ""));
  document.id = 9;

  EXPECT_TRUE(
      services->references.scopeComputation->collectExportedSymbols(document, {})
          .empty());
  EXPECT_TRUE(
      services->references.scopeComputation->collectLocalSymbols(document, {})
          .empty());
}

} // namespace
} // namespace pegium::references
