#include <gtest/gtest.h>

#include <algorithm>
#include <string>
#include <vector>

#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/DefaultTextDocuments.hpp>

namespace pegium::workspace {
namespace {

TEST(DefaultTextDocumentsTest, OpenStoresSnapshotAndEmitsOpenAndChangeEvents) {
  DefaultTextDocuments documents;

  std::vector<std::string> events;
  auto onDidOpen = documents.onDidOpen(
      [&events](const TextDocumentEvent &event) {
        events.push_back("open:" + event.document->uri);
      });
  auto onDidChange = documents.onDidChangeContent(
      [&events](const TextDocumentEvent &event) {
        events.push_back("change:" + event.document->uri);
      });

  (void)onDidOpen;
  (void)onDidChange;

  const auto uri = std::string("file:///default-text-documents.test");
  auto document = documents.open(uri, "test", "alpha", 7);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, uri);
  EXPECT_EQ(document->languageId, "test");
  EXPECT_EQ(document->text(), "alpha");
  EXPECT_EQ(document->clientVersion(), 7);

  const auto stored = documents.get(uri);
  ASSERT_NE(stored, nullptr);
  EXPECT_EQ(stored->text(), "alpha");
  EXPECT_EQ(events.size(), 2u);
  EXPECT_EQ(events[0], "open:" + uri);
  EXPECT_EQ(events[1], "change:" + uri);
}

TEST(DefaultTextDocumentsTest,
     WillSaveAndSaveNotifyListenersAndAggregateEdits) {
  DefaultTextDocuments documents;
  const auto uri = std::string("file:///default-text-documents-save.test");
  ASSERT_NE(documents.open(uri, "test", "alpha", 1), nullptr);

  TextDocumentSaveReason seenReason = TextDocumentSaveReason::Manual;
  std::vector<std::string> savedTexts;
  auto onWillSave = documents.onWillSave(
      [&seenReason](const TextDocumentWillSaveEvent &event) {
        seenReason = event.reason;
      });
  auto onWillSaveWaitUntilA = documents.onWillSaveWaitUntil(
      [](const TextDocumentWillSaveEvent &event) {
        std::vector<TextDocumentEdit> edits;
        if (event.document != nullptr) {
          TextDocumentEdit edit{};
          edit.range.start = text::Position(0, 0);
          edit.range.end = text::Position(0, 0);
          edit.newText = "prefix ";
          edits.push_back(std::move(edit));
        }
        return edits;
      });
  auto onWillSaveWaitUntilB = documents.onWillSaveWaitUntil(
      [](const TextDocumentWillSaveEvent &event) {
        std::vector<TextDocumentEdit> edits;
        if (event.document != nullptr) {
          TextDocumentEdit edit{};
          edit.range.start = text::Position(0, 5);
          edit.range.end = text::Position(0, 5);
          edit.newText = "!";
          edits.push_back(std::move(edit));
        }
        return edits;
      });
  auto onDidSave = documents.onDidSave(
      [&savedTexts](const TextDocumentEvent &event) {
        savedTexts.push_back(event.document->text());
      });

  (void)onWillSave;
  (void)onWillSaveWaitUntilA;
  (void)onWillSaveWaitUntilB;
  (void)onDidSave;

  EXPECT_TRUE(documents.willSave(uri, TextDocumentSaveReason::FocusOut));
  EXPECT_EQ(seenReason, TextDocumentSaveReason::FocusOut);

  const auto edits =
      documents.willSaveWaitUntil(uri, TextDocumentSaveReason::AfterDelay);
  ASSERT_EQ(edits.size(), 2u);
  EXPECT_TRUE(std::ranges::any_of(edits, [](const TextDocumentEdit &edit) {
    return edit.newText == "prefix ";
  }));
  EXPECT_TRUE(std::ranges::any_of(edits, [](const TextDocumentEdit &edit) {
    return edit.newText == "!";
  }));

  const auto saved = documents.save(uri, std::string("beta"));
  ASSERT_NE(saved, nullptr);
  EXPECT_EQ(saved->text(), "beta");
  ASSERT_EQ(savedTexts.size(), 1u);
  EXPECT_EQ(savedTexts.front(), "beta");
}

TEST(DefaultTextDocumentsTest, CloseRemovesSnapshotAndEmitsCloseEvent) {
  DefaultTextDocuments documents;
  const auto uri = std::string("file:///default-text-documents-close.test");
  ASSERT_NE(documents.open(uri, "test", "alpha"), nullptr);

  std::string closedUri;
  auto onDidClose = documents.onDidClose(
      [&closedUri](const TextDocumentEvent &event) {
        closedUri = event.document->uri;
      });
  (void)onDidClose;

  EXPECT_TRUE(documents.close(uri));
  EXPECT_EQ(closedUri, uri);
  EXPECT_EQ(documents.get(uri), nullptr);
  EXPECT_FALSE(documents.close(uri));
  EXPECT_FALSE(documents.willSave(uri, TextDocumentSaveReason::Manual));
  EXPECT_TRUE(documents.willSaveWaitUntil(uri, TextDocumentSaveReason::Manual)
                  .empty());
}

TEST(DefaultTextDocumentsTest, NormalizesFileUrisForStorageAndLookup) {
  DefaultTextDocuments documents;

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/default-text-documents-uri.test");
  const auto unnormalized = std::string(
      "file:///tmp/pegium-tests/folder/../default-text-documents-uri.test");

  auto document = documents.open(unnormalized, "test", "alpha", 1);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, normalized);
  EXPECT_EQ(documents.get(normalized), document);
  EXPECT_EQ(documents.get(unnormalized), document);
  EXPECT_TRUE(documents.close(normalized));
  EXPECT_EQ(documents.get(unnormalized), nullptr);
}

} // namespace
} // namespace pegium::workspace
