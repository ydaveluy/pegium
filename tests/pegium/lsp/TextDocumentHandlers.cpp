#include <gtest/gtest.h>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/workspace/DefaultTextDocuments.hpp>

namespace pegium {
namespace {

TEST(TextDocumentHandlersTest, NotificationsUpdateTextDocumentsStore) {
  DefaultTextDocuments documents;
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  auto disposable = documents.listen(handler);
  (void)disposable;

  const auto uri = test::make_file_uri("runtime-text-document.test");

  ::lsp::DidOpenTextDocumentParams openParams{};
  openParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  openParams.textDocument.languageId = "test";
  openParams.textDocument.version = 1;
  openParams.textDocument.text = "alpha";
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidOpen::Method,
      std::move(openParams)));
  handler.processIncomingMessages();

  auto opened = documents.get(uri);
  ASSERT_NE(opened, nullptr);
  EXPECT_EQ(opened->getText(), "alpha");
  EXPECT_EQ(opened->version(), 1);

  ::lsp::DidChangeTextDocumentParams changeParams{};
  changeParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  changeParams.textDocument.version = 2;
  ::lsp::TextDocumentContentChangeEvent_Text fullChange{};
  fullChange.text = "beta";
  changeParams.contentChanges.push_back(std::move(fullChange));
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidChange::Method,
      std::move(changeParams)));
  handler.processIncomingMessages();

  auto changed = documents.get(uri);
  ASSERT_NE(changed, nullptr);
  EXPECT_NE(changed.get(), opened.get());
  EXPECT_EQ(changed->getText(), "beta");
  EXPECT_EQ(changed->version(), 2);
  EXPECT_EQ(opened->getText(), "alpha");
  EXPECT_EQ(opened->version(), 1);

  ::lsp::DidSaveTextDocumentParams saveParams{};
  saveParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  saveParams.text = std::string("gamma");
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidSave::Method,
      std::move(saveParams)));
  handler.processIncomingMessages();

  auto saved = documents.get(uri);
  ASSERT_NE(saved, nullptr);
  EXPECT_EQ(saved->getText(), "beta");

  ::lsp::DidCloseTextDocumentParams closeParams{};
  closeParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_DidClose::Method,
      std::move(closeParams)));
  handler.processIncomingMessages();

  EXPECT_EQ(documents.get(uri), nullptr);
}

TEST(TextDocumentHandlersTest,
     WillSaveRequestsCallInitializationHookAndReturnEdits) {
  DefaultTextDocuments documents;
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  int initializedCalls = 0;
  auto disposable =
      documents.listen(handler, [&initializedCalls]() { ++initializedCalls; });
  (void)disposable;

  workspace::TextDocumentSaveReason seenReason =
      workspace::TextDocumentSaveReason::Manual;
  auto onWillSave = documents.onWillSave(
      [&seenReason](const workspace::TextDocumentWillSaveEvent &event) {
        seenReason = event.reason;
      });
  auto firstWillSaveWaitUntil = documents.onWillSaveWaitUntil(
      [](const workspace::TextDocumentWillSaveEvent &) {
        workspace::TextEdit edit{};
        edit.range.start = text::Position(0, 0);
        edit.range.end = text::Position(0, 0);
        edit.newText = "stale ";
        return std::vector<workspace::TextEdit>{std::move(edit)};
      });
  auto onWillSaveWaitUntil = documents.onWillSaveWaitUntil(
      [](const workspace::TextDocumentWillSaveEvent &) {
        workspace::TextEdit edit{};
        edit.range.start = text::Position(0, 0);
        edit.range.end = text::Position(0, 0);
        edit.newText = "prefix ";
        return std::vector<workspace::TextEdit>{std::move(edit)};
      });
  (void)onWillSave;
  (void)firstWillSaveWaitUntil;
  (void)onWillSaveWaitUntil;

  const auto uri = test::make_file_uri("runtime-will-save.test");
  ASSERT_NE(test::set_text_document(documents, uri, "test", "alpha", 1), nullptr);

  ::lsp::WillSaveTextDocumentParams willSaveParams{};
  willSaveParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  willSaveParams.reason = ::lsp::TextDocumentSaveReason::FocusOut;
  stream.pushInput(test::make_notification_message(
      ::lsp::notifications::TextDocument_WillSave::Method,
      std::move(willSaveParams)));
  handler.processIncomingMessages();

  EXPECT_EQ(initializedCalls, 1);
  EXPECT_EQ(seenReason, workspace::TextDocumentSaveReason::FocusOut);

  ::lsp::WillSaveTextDocumentParams waitUntilParams{};
  waitUntilParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  waitUntilParams.reason = ::lsp::TextDocumentSaveReason::AfterDelay;
  stream.pushInput(test::make_request_message(
      1, ::lsp::requests::TextDocument_WillSaveWaitUntil::Method,
      std::move(waitUntilParams)));
  handler.processIncomingMessages();

  EXPECT_EQ(initializedCalls, 2);
  EXPECT_NE(stream.written().find("\"id\":1"), std::string::npos);
  EXPECT_NE(stream.written().find("prefix "), std::string::npos);
  EXPECT_EQ(stream.written().find("stale "), std::string::npos);
}

} // namespace
} // namespace pegium
