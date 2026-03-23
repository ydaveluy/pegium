#include <gtest/gtest.h>

#include <ranges>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/DefaultDocuments.hpp>

namespace pegium::workspace {
namespace {

void register_test_language(services::SharedCoreServices &shared) {
  auto registeredServices =
      test::make_uninstalled_core_services(shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*registeredServices);
  shared.serviceRegistry->registerServices(std::move(registeredServices));
}

TEST(DefaultDocumentsTest, CreatesAndDeletesDocumentsViaFactory) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto *factory = new test::FakeDocumentFactory();
  factory->contentsByUri.try_emplace(test::make_file_uri("factory.test"),
                                     "from-uri");
  shared->workspace.documentFactory.reset(factory);

  auto created = shared->workspace.documents->createDocument(
      test::make_file_uri("created.test"), "created-text");
  EXPECT_NE(created->id, InvalidDocumentId);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(created->uri),
            created->id);
  EXPECT_EQ(shared->workspace.documents->getDocument(created->id), created);
  EXPECT_TRUE(shared->workspace.documents->hasDocument(created->uri));
  EXPECT_EQ(created->textDocument().getText(), "created-text");

  auto loaded = shared->workspace.documents->getOrCreateDocument(
      test::make_file_uri("factory.test"));
  EXPECT_NE(loaded->id, InvalidDocumentId);
  EXPECT_EQ(loaded->textDocument().getText(), "from-uri");

  auto deleted = shared->workspace.documents->deleteDocument(created->uri);
  ASSERT_NE(deleted, nullptr);
  EXPECT_EQ(deleted->state, DocumentState::Changed);
  EXPECT_FALSE(shared->workspace.documents->hasDocument(created->uri));
}

TEST(DefaultDocumentsTest, GetOrCreateDocumentPropagatesFactoryFailures) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  shared->workspace.documentFactory = std::make_unique<test::FakeDocumentFactory>();

  EXPECT_THROW(
      (void)shared->workspace.documents->getOrCreateDocument(
          test::make_file_uri("missing.test")),
      std::runtime_error);
}

TEST(DefaultDocumentsTest, RejectsDuplicateUris) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto textDocument = std::make_shared<TextDocument>(TextDocument::create(
      test::make_file_uri("duplicate.test"), "", 0, "text"));
  auto document = std::make_shared<Document>(std::move(textDocument));

  shared->workspace.documents->addDocument(document);
  EXPECT_THROW(shared->workspace.documents->addDocument(document),
               std::runtime_error);
}

TEST(DefaultDocumentsTest, RejectsNonNormalizedUrisOnAdd) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto textDocument = std::make_shared<TextDocument>(TextDocument::create(
      "file:///tmp/pegium-tests/folder/../documents-uri.test", "", 0, "text"));
  auto document = std::make_shared<Document>(std::move(textDocument));

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/documents-uri.test");

  EXPECT_THROW(shared->workspace.documents->addDocument(document),
               std::runtime_error);
  EXPECT_FALSE(shared->workspace.documents->hasDocument(normalized));
}

TEST(DefaultDocumentsTest, ReusesStableDocumentIdAcrossDeleteAndRecreate) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  register_test_language(*shared);
  const auto uri = test::make_file_uri("stable-id.test");

  auto first = shared->workspace.documents->createDocument(uri, "first");
  const auto firstId = first->id;
  ASSERT_NE(firstId, InvalidDocumentId);

  auto removed = shared->workspace.documents->deleteDocument(uri);
  ASSERT_NE(removed, nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(uri), firstId);

  auto second = shared->workspace.documents->createDocument(uri, "second");
  EXPECT_EQ(second->id, firstId);
}

TEST(DefaultDocumentsTest, DeleteDocumentByIdUsesUriBackedStorage) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  register_test_language(*shared);
  const auto uri = test::make_file_uri("delete-by-id.test");
  auto document = shared->workspace.documents->createDocument(uri, "content");
  const auto documentId = document->id;

  auto deleted = shared->workspace.documents->deleteDocument(documentId);
  ASSERT_EQ(deleted, document);
  EXPECT_EQ(document->state, DocumentState::Changed);
  EXPECT_EQ(shared->workspace.documents->getDocument(uri), nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocument(documentId), nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(uri), documentId);
}

TEST(DefaultDocumentsTest, GetAndDeleteDocumentsFollowFolderSubtree) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  register_test_language(*shared);
  const auto folderUri = test::make_file_uri("workspace");
  const auto childOneUri = test::make_file_uri("workspace/one.test");
  const auto childTwoUri = test::make_file_uri("workspace/nested/two.test");
  const auto otherUri = test::make_file_uri("outside.test");

  auto childOne = shared->workspace.documents->createDocument(childOneUri, "one");
  auto childTwo = shared->workspace.documents->createDocument(childTwoUri, "two");
  auto other = shared->workspace.documents->createDocument(otherUri, "other");

  auto documents = shared->workspace.documents->getDocuments(folderUri);
  ASSERT_EQ(documents.size(), 2u);
  EXPECT_TRUE(std::ranges::find(documents, childOne) != documents.end());
  EXPECT_TRUE(std::ranges::find(documents, childTwo) != documents.end());

  auto deleted = shared->workspace.documents->deleteDocuments(folderUri);
  ASSERT_EQ(deleted.size(), 2u);
  EXPECT_TRUE(std::ranges::find(deleted, childOne) != deleted.end());
  EXPECT_TRUE(std::ranges::find(deleted, childTwo) != deleted.end());
  EXPECT_EQ(shared->workspace.documents->getDocument(childOneUri), nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocument(childTwoUri), nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocument(otherUri), other);
}

TEST(DefaultDocumentsTest, CreateDocumentResolvesLanguageFromUri) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto uri = test::make_file_uri("resolved-language.test");
  auto document = shared->workspace.documents->createDocument(uri, "content");
  EXPECT_EQ(document->textDocument().languageId(), "test");
}

} // namespace
} // namespace pegium::workspace
