#include <gtest/gtest.h>

#include <string>
#include <typeindex>
#include <vector>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/references/ScopeProvider.hpp>

namespace pegium::references {
namespace {

struct TargetNode final : AstNode {};
struct BaseTargetNode : AstNode {};
struct DerivedTargetNode final : BaseTargetNode {};

struct RefHolder final : AstNode {
  Reference<TargetNode> ref;
};

struct BaseRefHolder final : AstNode {
  Reference<BaseTargetNode> ref;
};

std::vector<std::string> collect_names(const Scope &scope) {
  std::vector<std::string> names;
  for (const auto *entry : scope.getAllElements()) {
    EXPECT_NE(entry, nullptr);
    if (entry == nullptr) {
      continue;
    }
    names.push_back(entry->name);
  }
  return names;
}

TEST(DefaultScopeProviderTest, InvalidatesGlobalScopeCacheWhenDocumentChanges) {
  auto shared = test::make_shared_core_services();
  auto *builder = new test::RecordingEventDocumentBuilder();
  shared->workspace.documentBuilder.reset(builder);

  auto services = test::make_core_services(*shared, "test");
  auto *scopeProvider = services->references.scopeProvider.get();
  ASSERT_NE(scopeProvider, nullptr);

  RefHolder holder;
  holder.ref.initialize<TargetNode>(&holder, "first", std::nullopt, false);
  const auto uri = test::make_file_uri("scope-provider-change.test");
  const auto documentId = shared->workspace.documents->getOrCreateDocumentId(uri);
  const auto refType = std::type_index(typeid(TargetNode));

  shared->workspace.indexManager->setExports(
      documentId, {{.name = "first",
             .type = refType,
             .documentId = documentId}});

  auto initialScope = scopeProvider->getScope(makeReferenceInfo(holder.ref));
  ASSERT_NE(initialScope, nullptr);
  EXPECT_EQ(collect_names(*initialScope), (std::vector<std::string>{"first"}));

  shared->workspace.indexManager->setExports(
      documentId, {{.name = "second",
             .type = refType,
             .documentId = documentId}});
  builder->emitUpdate({documentId}, {});

  auto updatedScope = scopeProvider->getScope(makeReferenceInfo(holder.ref));
  ASSERT_NE(updatedScope, nullptr);
  EXPECT_EQ(collect_names(*updatedScope),
            (std::vector<std::string>{"second"}));
}

TEST(DefaultScopeProviderTest, InvalidatesGlobalScopeCacheWhenDocumentIsDeleted) {
  auto shared = test::make_shared_core_services();
  auto *builder = new test::RecordingEventDocumentBuilder();
  shared->workspace.documentBuilder.reset(builder);

  auto services = test::make_core_services(*shared, "test");
  auto *scopeProvider = services->references.scopeProvider.get();
  ASSERT_NE(scopeProvider, nullptr);

  RefHolder holder;
  holder.ref.initialize<TargetNode>(&holder, "value", std::nullopt, false);
  const auto uri = test::make_file_uri("scope-provider-delete.test");
  const auto documentId = shared->workspace.documents->getOrCreateDocumentId(uri);
  const auto refType = std::type_index(typeid(TargetNode));

  shared->workspace.indexManager->setExports(
      documentId, {{.name = "value",
             .type = refType,
             .documentId = documentId}});

  auto initialScope = scopeProvider->getScope(makeReferenceInfo(holder.ref));
  ASSERT_NE(initialScope, nullptr);
  EXPECT_EQ(collect_names(*initialScope), (std::vector<std::string>{"value"}));

  EXPECT_TRUE(shared->workspace.indexManager->remove(documentId));
  builder->emitUpdate({}, {documentId});

  auto updatedScope = scopeProvider->getScope(makeReferenceInfo(holder.ref));
  ASSERT_NE(updatedScope, nullptr);
  EXPECT_TRUE(collect_names(*updatedScope).empty());
}

TEST(DefaultScopeProviderTest,
     ExistingScopeObservesSubtypeLearnedWithoutRecreation) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test");
  auto *scopeProvider = services->references.scopeProvider.get();
  ASSERT_NE(scopeProvider, nullptr);
  ASSERT_NE(shared->astReflection.get(), nullptr);

  BaseRefHolder holder;
  holder.ref.initialize<BaseTargetNode>(&holder, "derived", std::nullopt, false);
  const auto uri = test::make_file_uri("scope-provider-dynamic-subtype.test");
  const auto documentId = shared->workspace.documents->getOrCreateDocumentId(uri);

  shared->workspace.indexManager->setExports(
      documentId, {{.name = "derived",
             .type = std::type_index(typeid(DerivedTargetNode)),
             .documentId = documentId}});

  auto scope = scopeProvider->getScope(makeReferenceInfo(holder.ref));
  ASSERT_NE(scope, nullptr);

  const auto knownBefore = shared->astReflection->lookupSubtype(
      std::type_index(typeid(DerivedTargetNode)),
      std::type_index(typeid(BaseTargetNode)));
  EXPECT_EQ(knownBefore, std::nullopt);

  shared->astReflection->registerSubtype(
      std::type_index(typeid(DerivedTargetNode)),
      std::type_index(typeid(BaseTargetNode)));

  const auto *resolved = scope->getElement("derived");
  ASSERT_NE(resolved, nullptr);
  EXPECT_EQ(resolved->name, "derived");
}

} // namespace
} // namespace pegium::references
