#include <gtest/gtest.h>

#include <array>
#include <algorithm>
#include <memory>
#include <string>
#include <typeindex>
#include <unordered_map>
#include <vector>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/AstReflectionBootstrap.hpp>
#include <pegium/core/references/ScopeComputation.hpp>
#include <pegium/core/workspace/DefaultIndexManager.hpp>
#include <pegium/core/workspace/ReferenceDescriptionProvider.hpp>

namespace pegium::workspace {
namespace {

using namespace pegium::parser;

struct BaseNode : AstNode {};
struct DerivedNode final : BaseNode {};
struct OtherNode final : AstNode {};

class IndexSubtypeBootstrapParser final : public PegiumParser {
protected:
  const grammar::ParserRule &getEntryRule() const noexcept override {
    return BaseRule;
  }

  Rule<DerivedNode> DerivedRule{"Derived", "derived"_kw};
  Rule<BaseNode> BaseRule{"Base", DerivedRule};
};

class TestScopeComputation final : public references::ScopeComputation {
public:
  std::unordered_map<DocumentId, std::vector<AstNodeDescription>>
      exportsByDocument;

  std::vector<AstNodeDescription>
  collectExportedSymbols(const Document &document,
                         const utils::CancellationToken &) const override {
    if (const auto it = exportsByDocument.find(document.id);
        it != exportsByDocument.end()) {
      return it->second;
    }
    return {};
  }

  LocalSymbols collectLocalSymbols(
      const Document &,
      const utils::CancellationToken &) const override {
    return {};
  }
};

class TestReferenceDescriptionProvider final
    : public ReferenceDescriptionProvider {
public:
  std::unordered_map<DocumentId, std::vector<ReferenceDescription>>
      referencesByDocument;

  std::vector<ReferenceDescription>
  createDescriptions(const Document &document,
                     const utils::CancellationToken &) const override {
    if (const auto it = referencesByDocument.find(document.id);
        it != referencesByDocument.end()) {
      return it->second;
    }
    return {};
  }
};

std::vector<std::string>
collect_names(const std::vector<AstNodeDescription> &entries) {
  std::vector<std::string> names;
  for (const auto &entry : entries) {
    names.push_back(entry.name);
  }
  std::ranges::sort(names);
  return names;
}

std::unique_ptr<Document> make_document(DocumentId documentId,
                                        std::string languageId = "test") {
  auto document = std::make_unique<Document>(test::make_text_document(
      test::make_file_uri("index-" + std::to_string(documentId) + ".test"),
      languageId, {}));
  document->id = documentId;
  return document;
}

TEST(DefaultIndexManagerTest, FiltersElementsBySubtypeAndDocumentSubset) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);
  IndexSubtypeBootstrapParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         *shared->astReflection);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto scopeComputation = std::make_unique<TestScopeComputation>();
  auto *scopeComputationPtr = scopeComputation.get();
  services->references.scopeComputation = std::move(scopeComputation);
  shared->serviceRegistry->registerServices(std::move(services));

  scopeComputationPtr->exportsByDocument[1] = {
      {.name = "derived",
       .type = std::type_index(typeid(DerivedNode)),
       .documentId = 1},
  };
  scopeComputationPtr->exportsByDocument[2] = {
      {.name = "base",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 2},
      {.name = "other",
       .type = std::type_index(typeid(OtherNode)),
       .documentId = 2},
  };

  auto firstDocument = make_document(1);
  auto secondDocument = make_document(2);
  indexManager.updateContent(*firstDocument, {});
  indexManager.updateContent(*secondDocument, {});

  EXPECT_EQ(collect_names(indexManager.allElements()),
            (std::vector<std::string>{"base", "derived", "other"}));
  EXPECT_EQ(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode)))),
      (std::vector<std::string>{"base", "derived"}));

  const std::array<DocumentId, 1> documentIds{2};
  EXPECT_EQ(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode)),
                                             documentIds)),
      (std::vector<std::string>{"base"}));
}

TEST(DefaultIndexManagerTest, FindByNameReturnsFirstMatchingDescription) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);
  IndexSubtypeBootstrapParser parser;
  bootstrapAstReflection(static_cast<const Parser &>(parser).getEntryRule(),
                         *shared->astReflection);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto scopeComputation = std::make_unique<TestScopeComputation>();
  auto *scopeComputationPtr = scopeComputation.get();
  services->references.scopeComputation = std::move(scopeComputation);
  shared->serviceRegistry->registerServices(std::move(services));

  scopeComputationPtr->exportsByDocument[1] = {
      {.name = "derived",
       .type = std::type_index(typeid(DerivedNode)),
       .documentId = 1},
      {.name = "shared",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 1},
  };
  scopeComputationPtr->exportsByDocument[2] = {
      {.name = "base",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 2},
      {.name = "other",
       .type = std::type_index(typeid(OtherNode)),
       .documentId = 2},
      {.name = "shared",
       .type = std::type_index(typeid(OtherNode)),
       .documentId = 2},
  };

  auto firstDocument = make_document(1);
  auto secondDocument = make_document(2);
  indexManager.updateContent(*firstDocument, {});
  indexManager.updateContent(*secondDocument, {});

  const auto base = indexManager.findByName("base");
  ASSERT_TRUE(base.has_value());
  EXPECT_EQ(base->name, "base");
  EXPECT_EQ(base->documentId, 2);

  EXPECT_FALSE(indexManager.findByName("missing").has_value());

  // First match wins in allElements() order (documents sorted by id), so the
  // duplicate "shared" resolves to document 1, not document 2.
  const auto sharedEntry = indexManager.findByName("shared");
  ASSERT_TRUE(sharedEntry.has_value());
  EXPECT_EQ(sharedEntry->documentId, 1);

  // Type filter mirrors allElements(type).
  EXPECT_TRUE(indexManager.findByName("base", std::type_index(typeid(BaseNode)))
                  .has_value());
  EXPECT_FALSE(indexManager.findByName("other", std::type_index(typeid(BaseNode)))
                   .has_value());

  // Skip-and-continue: a typed query must skip doc1's BaseNode "shared" (fails
  // the OtherNode filter) and keep scanning to doc2's OtherNode "shared" — this
  // fails a naive return-nullopt-on-first-name-match implementation.
  const auto sharedTyped =
      indexManager.findByName("shared", std::type_index(typeid(OtherNode)));
  ASSERT_TRUE(sharedTyped.has_value());
  EXPECT_EQ(sharedTyped->documentId, 2);
}

TEST(DefaultIndexManagerTest, InvalidatesTypedExportCacheOnContentUpdateAndRemove) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto scopeComputation = std::make_unique<TestScopeComputation>();
  auto *scopeComputationPtr = scopeComputation.get();
  services->references.scopeComputation = std::move(scopeComputation);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = make_document(1);
  scopeComputationPtr->exportsByDocument[1] = {
      {.name = "first",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 1},
  };
  indexManager.updateContent(*document, {});
  EXPECT_EQ(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode)))),
      (std::vector<std::string>{"first"}));

  scopeComputationPtr->exportsByDocument[1] = {
      {.name = "second",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 1},
  };
  indexManager.updateContent(*document, {});
  EXPECT_EQ(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode)))),
      (std::vector<std::string>{"second"}));

  EXPECT_TRUE(indexManager.removeContent(1));
  EXPECT_TRUE(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode))))
          .empty());
}

TEST(DefaultIndexManagerTest, PreservesDocumentIdProvidedByDescriptions) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto scopeComputation = std::make_unique<TestScopeComputation>();
  auto *scopeComputationPtr = scopeComputation.get();
  services->references.scopeComputation = std::move(scopeComputation);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = make_document(7);
  scopeComputationPtr->exportsByDocument[7] = {
      {.name = "entry",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 99},
  };

  indexManager.updateContent(*document, {});

  auto entries = indexManager.allElements();
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front().documentId, 99u);
}

TEST(DefaultIndexManagerTest, ResolvesServicesFromUriInsteadOfDocumentLanguageId) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto scopeComputation = std::make_unique<TestScopeComputation>();
  auto *scopeComputationPtr = scopeComputation.get();
  services->references.scopeComputation = std::move(scopeComputation);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = make_document(1, "wrong-language");
  scopeComputationPtr->exportsByDocument[1] = {
      {.name = "entry",
       .type = std::type_index(typeid(BaseNode)),
       .documentId = 1},
  };

  EXPECT_NO_THROW(indexManager.updateContent(*document, {}));
  EXPECT_EQ(
      collect_names(indexManager.allElements(std::type_index(typeid(BaseNode)))),
      (std::vector<std::string>{"entry"}));
}

TEST(DefaultIndexManagerTest,
     FindAllReferencesReturnsUsagesAndIsAffectedIgnoresLocalReferences) {
  auto shared = test::make_empty_shared_core_services();
  pegium::installDefaultSharedCoreServices(*shared);
  DefaultIndexManager indexManager(*shared);

  auto services =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  auto referenceProvider = std::make_unique<TestReferenceDescriptionProvider>();
  auto *referenceProviderPtr = referenceProvider.get();
  services->workspace.referenceDescriptionProvider = std::move(referenceProvider);
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = make_document(1);
  referenceProviderPtr->referencesByDocument[1] = {
      {.sourceDocumentId = 1,
       .local = true,
       .targetDocumentId = 1,
       .targetSymbolId = 11},
      {.sourceDocumentId = 1,
       .local = false,
       .targetDocumentId = 2,
       .targetSymbolId = 22},
  };

  indexManager.updateReferences(*document, {});

  auto storedReferences =
      indexManager.findAllReferences(NodeKey{.documentId = 1, .symbolId = 11});
  ASSERT_EQ(storedReferences.size(), 1u);
  EXPECT_TRUE(storedReferences.front().local);
  EXPECT_EQ(storedReferences.front().sourceDocumentId, 1u);

  auto references =
      indexManager.findAllReferences(NodeKey{.documentId = 2, .symbolId = 22});
  ASSERT_EQ(references.size(), 1u);
  EXPECT_EQ(references.front().sourceDocumentId, 1u);
  EXPECT_FALSE(references.front().local);

  EXPECT_FALSE(indexManager.isAffected(*document, {1}));
  EXPECT_TRUE(indexManager.isAffected(*document, {2}));

  EXPECT_TRUE(indexManager.removeReferences(1));
  EXPECT_TRUE(indexManager.findAllReferences(
                  NodeKey{.documentId = 2, .symbolId = 22})
                  .empty());
}

} // namespace
} // namespace pegium::workspace
