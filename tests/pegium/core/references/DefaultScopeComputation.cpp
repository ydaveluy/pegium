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
  std::unique_ptr<ScopeLeaf> directLeaf;
  std::vector<std::unique_ptr<ScopeLeaf>> leaves;
};

struct ScopeRoot final : NamedScopeNode {
  using NamedScopeNode::NamedScopeNode;
  std::unique_ptr<ScopeBranch> branch;
  std::vector<std::unique_ptr<ScopeLeaf>> leaves;
};

class TestNameProvider final : public NameProvider {
public:
  [[nodiscard]] std::optional<std::string>
  getName(const AstNode &node) const noexcept override {
    if (const auto *named = dynamic_cast<const NamedScopeNode *>(&node)) {
      if (!named->name.empty()) {
        return named->name;
      }
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<CstNodeView>
  getNameNode(const AstNode &) const noexcept override {
    return std::nullopt;
  }
};

class PrefixNameProvider final : public NameProvider {
public:
  explicit PrefixNameProvider(std::string prefix) : _prefix(std::move(prefix)) {}

  [[nodiscard]] std::optional<std::string>
  getName(const AstNode &node) const noexcept override {
    if (const auto *named = dynamic_cast<const NamedScopeNode *>(&node);
        named != nullptr && !named->name.empty()) {
      return _prefix + named->name;
    }
    return std::nullopt;
  }

  [[nodiscard]] std::optional<CstNodeView>
  getNameNode(const AstNode &) const noexcept override {
    return std::nullopt;
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
                    std::string name) const override {
    if (name.empty()) {
      return std::nullopt;
    }
    auto fullName = _prefix + name;

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
        .nameLength = static_cast<TextOffset>(_prefix.size() + name.size()),
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

  auto root = std::make_unique<ScopeRoot>("root");
  root->branch = std::make_unique<ScopeBranch>("branch");
  root->leaves.push_back(std::make_unique<ScopeLeaf>("leaf"));
  root->leaves.push_back(std::make_unique<ScopeLeaf>());
  root->branch->directLeaf = std::make_unique<ScopeLeaf>("nested");
  root->branch->leaves.push_back(std::make_unique<ScopeLeaf>("nested2"));

  root->branch->setContainer<ScopeRoot, &ScopeRoot::branch>(*root);
  root->leaves[0]->setContainer<ScopeRoot, &ScopeRoot::leaves>(*root, 0);
  root->leaves[1]->setContainer<ScopeRoot, &ScopeRoot::leaves>(*root, 1);
  root->branch->directLeaf->setContainer<ScopeBranch, &ScopeBranch::directLeaf>(
      *root->branch);
  root->branch->leaves[0]->setContainer<ScopeBranch, &ScopeBranch::leaves>(
      *root->branch, 0);

  document->parseResult.value = std::move(root);
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
  for (const auto &[container, description] : symbols) {
    (void)container;
    names.push_back(description.name);
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
