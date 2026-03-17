#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <typeindex>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/syntax-tree/Reference.hpp>
#include <pegium/workspace/DefaultReferenceDescriptionProvider.hpp>

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

TEST(DefaultReferenceDescriptionProviderTest,
     UsesResolvedTargetDescriptionsWithoutIntermediateTargetStrings) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test");
  auto *provider = services->workspace.referenceDescriptionProvider.get();
  ASSERT_NE(provider, nullptr);

  auto document = std::make_shared<Document>();
  document->uri = test::make_file_uri("reference-description-provider.test");
  document->id = 7;
  document->state = DocumentState::Linked;
  document->setText("alpha");

  auto root = std::make_unique<RootNode>();

  auto target = std::make_unique<TargetNode>();
  target->name = "alpha";
  auto *targetPtr = target.get();
  root->targets.push_back(std::move(target));
  targetPtr->setContainer<RootNode, &RootNode::targets>(*root, 0);

  auto ref = std::make_unique<RefNode>();
  auto *refPtr = ref.get();
  refPtr->target.initialize<TargetNode>(refPtr, "alpha", std::nullopt, false);
  root->refs.push_back(std::move(ref));
  refPtr->setContainer<RootNode, &RootNode::refs>(*root, 0);

  document->parseResult.value = std::move(root);
  document->parseResult.references.push_back(
      ReferenceHandle::direct(&refPtr->target));
  document->references = document->parseResult.references;

  AstNodeDescription targetDescription{
      .name = "alpha",
      .node = targetPtr,
      .type = std::type_index(typeid(TargetNode)),
      .documentId = document->id,
      .symbolId = 17,
  };
  document->references.front().get()->setResolution(
      ReferenceResolution{.node = targetPtr, .description = &targetDescription});

  const auto descriptions = provider->createDescriptions(*document);
  ASSERT_EQ(descriptions.size(), 1u);
  EXPECT_EQ(descriptions.front().sourceText(document->textView()), "alpha");
  ASSERT_TRUE(descriptions.front().targetDocumentId.has_value());
  ASSERT_TRUE(descriptions.front().targetSymbolId.has_value());
  EXPECT_EQ(*descriptions.front().targetDocumentId, document->id);
  EXPECT_EQ(*descriptions.front().targetSymbolId, targetDescription.symbolId);
}

} // namespace
} // namespace pegium::workspace
