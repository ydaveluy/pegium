#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/core/CoreTestSupport.hpp>
#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/workspace/DefaultTextDocuments.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium {
namespace {

TEST(DefaultTextDocumentsTest, SetStoresSnapshotAndEmitsOpenAndChangeEvents) {
  DefaultTextDocuments documents;

  std::vector<std::string> events;
  auto onDidOpen = documents.onDidOpen(
      [&events](const workspace::TextDocumentChangeEvent &event) {
        events.push_back("open:" + event.document->uri());
      });
  auto onDidChange = documents.onDidChangeContent(
      [&events](const workspace::TextDocumentChangeEvent &event) {
        events.push_back("change:" + event.document->uri());
      });

  (void)onDidOpen;
  (void)onDidChange;

  const auto uri = std::string("file:///default-text-documents.test");
  auto document = test::set_text_document(documents, uri, "test", "alpha", 7);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri(), uri);
  EXPECT_EQ(document->languageId(), "test");
  EXPECT_EQ(document->getText(), "alpha");
  EXPECT_EQ(document->version(), 7);

  const auto stored = documents.get(uri);
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(stored->getText(), "alpha");
  EXPECT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0], "open:" + uri);
  EXPECT_EQ(events[1], "change:" + uri);
}

TEST(DefaultTextDocumentsTest, SetExistingUriReturnsFalseAndReemitsOpenAndChange) {
  DefaultTextDocuments documents;
  const auto uri = std::string("file:///default-text-documents-replace.test");

  std::vector<std::string> events;
  auto onDidOpen = documents.onDidOpen(
      [&events](const workspace::TextDocumentChangeEvent &event) {
        events.push_back("open:" + std::string(event.document->getText()));
      });
  auto onDidChange = documents.onDidChangeContent(
      [&events](const workspace::TextDocumentChangeEvent &event) {
        events.push_back("change:" + std::string(event.document->getText()));
      });

  (void)onDidOpen;
  (void)onDidChange;

  ASSERT_NE(test::set_text_document(documents, uri, "test", "alpha", 1), nullptr);

  auto replacement = std::make_shared<workspace::TextDocument>(
      workspace::TextDocument::create(uri, "test", 2, "beta"));

  EXPECT_FALSE(documents.set(replacement));

  auto stored = documents.get(uri);
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(stored->getText(), "beta");
  EXPECT_EQ(events, (std::vector<std::string>{
                        "open:alpha", "change:alpha", "open:beta", "change:beta"}));
}

TEST(DefaultTextDocumentsTest,
     ListenReplacesStoredSnapshotInsteadOfMutatingExistingOpenDocument) {
  DefaultTextDocuments documents;
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);
  auto subscription = documents.listen(handler);
  (void)subscription;

  const auto uri = std::string("file:///default-text-documents-listen.test");

  ::lsp::DidOpenTextDocumentParams openParams{};
  openParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  openParams.textDocument.languageId = "test";
  openParams.textDocument.version = 1;
  openParams.textDocument.text = "alpha";
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidOpen::Method, std::move(openParams)));
  handler.processIncomingMessages();

  auto original = documents.get(uri);
  ASSERT_NE(original, nullptr);
  EXPECT_EQ(original->getText(), "alpha");
  EXPECT_EQ(original->version(), 1);

  ::lsp::DidChangeTextDocumentParams changeParams{};
  changeParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  changeParams.textDocument.version = 2;
  changeParams.contentChanges.push_back(
      ::lsp::TextDocumentContentChangeEvent_Text{.text = "beta"});
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidChange::Method,
      std::move(changeParams)));
  handler.processIncomingMessages();

  auto updated = documents.get(uri);
  ASSERT_NE(updated, nullptr);
  EXPECT_NE(updated.get(), original.get());
  EXPECT_EQ(updated->getText(), "beta");
  EXPECT_EQ(updated->version(), 2);
  EXPECT_EQ(original->getText(), "alpha");
  EXPECT_EQ(original->version(), 1);
}

TEST(DefaultTextDocumentsTest, RemoveRemovesSnapshotAndEmitsCloseEvent) {
  DefaultTextDocuments documents;
  const auto uri = std::string("file:///default-text-documents-close.test");
  ASSERT_NE(test::set_text_document(documents, uri, "test", "alpha"), nullptr);

  std::string closedUri;
  auto onDidClose = documents.onDidClose(
      [&closedUri](const workspace::TextDocumentChangeEvent &event) {
        closedUri = event.document->uri();
      });
  (void)onDidClose;

  documents.remove(uri);
  EXPECT_EQ(closedUri, uri);
  EXPECT_EQ(documents.get(uri), nullptr);
  documents.remove(uri);
  EXPECT_EQ(documents.get(uri), nullptr);
}

TEST(DefaultTextDocumentsTest, LookupAndRemoveNormalizeFileUris) {
  DefaultTextDocuments documents;

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/default-text-documents-uri.test");
  const auto unnormalized = std::string(
      "file:///tmp/pegium-tests/folder/../default-text-documents-uri.test");

  auto document =
      test::set_text_document(documents, unnormalized, "test", "alpha", 1);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri(), normalized);
  EXPECT_EQ(documents.get(normalized), document);
  EXPECT_EQ(documents.get(unnormalized), document);
  documents.remove(normalized);
  EXPECT_EQ(documents.get(unnormalized), nullptr);
}

TEST(DefaultTextDocumentsTest, AllAndKeysReflectCurrentState) {
  DefaultTextDocuments documents;

  const auto firstUri = std::string("file:///default-text-documents-all-a.test");
  const auto secondUri = std::string("file:///default-text-documents-all-b.test");
  ASSERT_NE(test::set_text_document(documents, firstUri, "test", "alpha"), nullptr);
  ASSERT_NE(test::set_text_document(documents, secondUri, "test", "beta"), nullptr);

  const auto allDocuments = documents.all();
  ASSERT_EQ(allDocuments.size(), 2u);
  EXPECT_TRUE(std::ranges::any_of(allDocuments, [&firstUri](const auto &document) {
    return document->uri() == firstUri;
  }));
  EXPECT_TRUE(std::ranges::any_of(allDocuments, [&secondUri](const auto &document) {
    return document->uri() == secondUri;
  }));

  const auto uris = documents.keys();
  ASSERT_EQ(uris.size(), 2u);
  EXPECT_NE(std::ranges::find(uris, firstUri), uris.end());
  EXPECT_NE(std::ranges::find(uris, secondUri), uris.end());
}

TEST(DefaultTextDocumentsTest, WillSaveWaitUntilSubscriptionCanOutliveStore) {
  utils::ScopedDisposable subscription;
  {
    auto documents = std::make_unique<DefaultTextDocuments>();
    subscription = documents->onWillSaveWaitUntil(
        [](const workspace::TextDocumentWillSaveEvent &) {
          return std::vector<workspace::TextEdit>{};
        });
  }

  EXPECT_FALSE(subscription.disposed());
}

} // namespace
} // namespace pegium
