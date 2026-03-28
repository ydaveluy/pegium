#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <typeindex>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/syntax-tree/Reference.hpp>
#include <pegium/core/workspace/DefaultReferenceDescriptionProvider.hpp>

namespace pegium::workspace {
namespace {

struct TargetNode final : AstNode {
  string name;
};

struct RefNode final : AstNode {
  reference<TargetNode> target;
};

struct RootNode final : AstNode {
  vector<pointer<TargetNode>> targets;
  vector<pointer<RefNode>> refs;
};

template <typename OwnerType, typename TargetType>
struct TestReferenceAssignment final : grammar::Assignment {
  explicit TestReferenceAssignment(std::string_view feature) noexcept
      : feature(feature) {}

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }
  void execute(AstNode *, const CstNodeView &,
               const parser::ValueBuildContext &) const override {}
  [[nodiscard]] grammar::FeatureValue
  getValue(const AstNode *) const override {
    return {};
  }
  [[nodiscard]] const grammar::AbstractElement *
  getElement() const noexcept override {
    return nullptr;
  }
  [[nodiscard]] std::string_view getFeature() const noexcept override {
    return feature;
  }
  [[nodiscard]] bool isReference() const noexcept override { return true; }
  [[nodiscard]] bool isMultiReference() const noexcept override {
    return false;
  }
  [[nodiscard]] std::type_index getType() const noexcept override {
    return std::type_index(typeid(TargetType));
  }

  std::string_view feature;
};

class StaticScopeProvider final : public references::ScopeProvider {
public:
  explicit StaticScopeProvider(std::vector<AstNodeDescription> entries)
      : _entries(std::move(entries)) {}

  const AstNodeDescription *getScopeEntry(const ReferenceInfo &context) const override {
    for (const auto &entry : _entries) {
      if (entry.name == context.referenceText) {
        return std::addressof(entry);
      }
    }
    return nullptr;
  }

private:
  bool visitScopeEntries(
      const ReferenceInfo &context,
      utils::function_ref<bool(const AstNodeDescription &)> visitor) const override {
    for (const auto &entry : _entries) {
      if (!context.referenceText.empty() && entry.name != context.referenceText) {
        continue;
      }
      if (!visitor(entry)) {
        return false;
      }
    }
    return true;
  }

  std::vector<AstNodeDescription> _entries;
};

struct DummyLiteral final : grammar::Literal {
  [[nodiscard]] bool isNullable() const noexcept override { return false; }

  [[nodiscard]] std::string_view getValue() const noexcept override {
    return "alpha";
  }

  [[nodiscard]] bool isCaseSensitive() const noexcept override { return true; }
};

TEST(DefaultReferenceDescriptionProviderTest,
     UsesResolvedTargetDescriptionsWithoutIntermediateTargetStrings) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::installDefaultCoreServices(*services);
  auto *linker = services->references.linker.get();
  auto *provider = services->workspace.referenceDescriptionProvider.get();
  ASSERT_NE(linker, nullptr);
  ASSERT_NE(provider, nullptr);
  TestReferenceAssignment<RefNode, TargetNode> assignment("target");

  auto textDocument = std::make_shared<TextDocument>(TextDocument::create(
      test::make_file_uri("reference-description-provider.test"), "", 0,
      "alpha alpha"));
  auto document = std::make_shared<Document>(textDocument);
  document->state = DocumentState::Linked;
  shared->workspace.documents->addDocument(document);

  auto root = std::make_unique<RootNode>();
  auto cst = std::make_unique<RootCstNode>(text::TextSnapshot::copy(document->textDocument().getText()));
  cst->attachDocument(*document);
  static const DummyLiteral literal;
  CstBuilder builder(*cst);
  builder.leaf(0, 5, &literal);
  builder.leaf(6, 11, &literal);

  auto target = std::make_unique<TargetNode>();
  target->name = "alpha";
  auto *targetPtr = target.get();
  targetPtr->setCstNode(cst->get(0));
  root->targets.push_back(std::move(target));
  targetPtr->setContainer<RootNode, &RootNode::targets>(*root, 0);

  auto ref = std::make_unique<RefNode>();
  auto *refPtr = ref.get();
  refPtr->setCstNode(cst->get(1));
  refPtr->target.initialize(
      *refPtr, "alpha", std::nullopt, assignment, *linker);
  root->refs.push_back(std::move(ref));
  refPtr->setContainer<RootNode, &RootNode::refs>(*root, 0);

  root->setCstNode(cst->get(1));
  document->parseResult.value = std::move(root);
  document->parseResult.cst = std::move(cst);
  document->parseResult.references.push_back(
      ReferenceHandle::direct(&refPtr->target));
  document->references = document->parseResult.references;
  const auto targetSymbolId = document->makeSymbolId(*targetPtr);

  AstNodeDescription targetDescription{
      .name = "alpha",
      .type = std::type_index(typeid(TargetNode)),
      .documentId = document->id,
      .symbolId = targetSymbolId,
  };
  services->references.scopeProvider =
      std::make_unique<StaticScopeProvider>(std::vector<AstNodeDescription>{
          targetDescription});

  const auto *resolved = refPtr->target.get();
  ASSERT_EQ(resolved, targetPtr);

  const auto descriptions = provider->createDescriptions(*document);
  ASSERT_EQ(descriptions.size(), 1u);
  EXPECT_EQ(descriptions.front().sourceText(document->textDocument().getText()),
            "alpha");
  ASSERT_TRUE(descriptions.front().targetDocumentId.has_value());
  ASSERT_TRUE(descriptions.front().targetSymbolId.has_value());
  EXPECT_TRUE(descriptions.front().local);
  EXPECT_EQ(*descriptions.front().targetDocumentId, document->id);
  EXPECT_EQ(*descriptions.front().targetSymbolId, targetSymbolId);
}

} // namespace
} // namespace pegium::workspace
