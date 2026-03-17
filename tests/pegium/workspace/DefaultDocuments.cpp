#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/DefaultDocuments.hpp>

namespace pegium::workspace {
namespace {

TEST(DefaultDocumentsTest, CreatesAndDeletesDocumentsViaFactory) {
  auto shared = test::make_shared_core_services();
  auto *factory = new test::FakeDocumentFactory();
  factory->contentsByUri.emplace(test::make_file_uri("factory.test"), "from-uri");
  shared->workspace.documentFactory.reset(factory);

  auto created = shared->workspace.documents->createDocument(
      test::make_file_uri("created.test"), "created-text", "test");
  ASSERT_NE(created, nullptr);
  EXPECT_NE(created->id, InvalidDocumentId);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(created->uri), created->id);
  EXPECT_EQ(shared->workspace.documents->getDocument(created->id), created);
  EXPECT_TRUE(shared->workspace.documents->hasDocument(created->uri));
  EXPECT_EQ(created->text(), "created-text");

  auto loaded =
      shared->workspace.documents->getOrCreateDocument(test::make_file_uri("factory.test"));
  ASSERT_NE(loaded, nullptr);
  EXPECT_NE(loaded->id, InvalidDocumentId);
  EXPECT_EQ(loaded->text(), "from-uri");

  auto deleted = shared->workspace.documents->deleteDocument(created->uri);
  ASSERT_NE(deleted, nullptr);
  EXPECT_EQ(deleted->state, DocumentState::Changed);
  EXPECT_FALSE(shared->workspace.documents->hasDocument(created->uri));
}

TEST(DefaultDocumentsTest, RejectsDuplicateUris) {
  auto shared = test::make_shared_core_services();

  auto document = std::make_shared<Document>();
  document->uri = test::make_file_uri("duplicate.test");
  document->replaceText("text");

  shared->workspace.documents->addDocument(document);
  EXPECT_THROW(shared->workspace.documents->addDocument(document),
               std::runtime_error);
}

TEST(DefaultDocumentsTest, NormalizesUrisOnAddAndLookup) {
  auto shared = test::make_shared_core_services();

  auto document = std::make_shared<Document>();
  document->uri = "file:///tmp/pegium-tests/folder/../documents-uri.test";
  document->replaceText("text");

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/documents-uri.test");

  shared->workspace.documents->addDocument(document);

  EXPECT_EQ(document->uri, normalized);
  EXPECT_NE(document->id, InvalidDocumentId);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(normalized), document->id);
  EXPECT_TRUE(shared->workspace.documents->hasDocument(normalized));
  EXPECT_TRUE(shared->workspace.documents->hasDocument(
      "file:///tmp/pegium-tests/folder/../documents-uri.test"));
  EXPECT_EQ(shared->workspace.documents->getDocument(document->id), document);
}

TEST(DefaultDocumentsTest, ReusesStableDocumentIdAcrossDeleteAndRecreate) {
  auto shared = test::make_shared_core_services();
  const auto uri = test::make_file_uri("stable-id.test");

  auto first = shared->workspace.documents->createDocument(uri, "first", "test");
  ASSERT_NE(first, nullptr);
  const auto firstId = first->id;
  ASSERT_NE(firstId, InvalidDocumentId);

  auto removed = shared->workspace.documents->deleteDocument(uri);
  ASSERT_NE(removed, nullptr);
  EXPECT_EQ(shared->workspace.documents->getDocumentId(uri), firstId);

  auto second = shared->workspace.documents->createDocument(uri, "second", "test");
  ASSERT_NE(second, nullptr);
  EXPECT_EQ(second->id, firstId);
}

TEST(DefaultDocumentsTest, InvalidateDocumentResetsStateToChanged) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  const auto uri = test::make_file_uri("invalidate.test");
  auto document = shared->workspace.documents->createDocument(uri, "content", "test");
  ASSERT_NE(document, nullptr);
  BuildOptions options;
  options.validation.enabled = true;
  ASSERT_TRUE(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options));
  ASSERT_EQ(document->state, DocumentState::Validated);

  auto invalidated = shared->workspace.documents->invalidateDocument(uri);
  ASSERT_EQ(invalidated, document);
  EXPECT_EQ(document->state, DocumentState::Changed);
  EXPECT_TRUE(document->diagnostics.empty());
}

} // namespace
} // namespace pegium::workspace
