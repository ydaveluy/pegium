#include <gtest/gtest.h>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/FileOperationHandler.hpp>
#include <pegium/lsp/LanguageServerRuntime.hpp>

namespace pegium::lsp {
namespace {

class RecordingRuntimeDocumentUpdateHandler final : public DocumentUpdateHandler {
public:
  bool supportsDidOpenDocument() const noexcept override { return true; }

  bool supportsDidChangeContent() const noexcept override { return true; }

  bool supportsDidSaveDocument() const noexcept override { return true; }

  bool supportsDidCloseDocument() const noexcept override { return true; }

  bool supportsWillSaveDocument() const noexcept override { return true; }

  bool supportsWillSaveDocumentWaitUntil() const noexcept override {
    return true;
  }

  bool supportsDidChangeWatchedFiles() const noexcept override { return true; }

  void didOpenDocument(const TextDocumentChangeEvent &event) override {
    openedUris.push_back(event.document->uri);
  }

  void didChangeContent(const TextDocumentChangeEvent &event) override {
    changedUris.push_back(event.document->uri);
  }

  void willSaveDocument(const TextDocumentWillSaveEvent &event) override {
    willSaveReasons.push_back(event.reason);
  }

  ::lsp::Array<::lsp::TextEdit>
  willSaveDocumentWaitUntil(const TextDocumentWillSaveEvent &event) override {
    waitUntilReasons.push_back(event.reason);
    ::lsp::TextEdit edit{};
    edit.range.start = text::Position(0, 0);
    edit.range.end = text::Position(0, 0);
    edit.newText = "runtime ";
    return {edit};
  }

  void didSaveDocument(const TextDocumentChangeEvent &event) override {
    savedUris.push_back(event.document->uri);
  }

  void didCloseDocument(const TextDocumentChangeEvent &event) override {
    closedUris.push_back(event.document->uri);
  }

  std::vector<std::string> openedUris;
  std::vector<std::string> changedUris;
  std::vector<std::string> savedUris;
  std::vector<std::string> closedUris;
  std::vector<::lsp::TextDocumentSaveReason> willSaveReasons;
  std::vector<::lsp::TextDocumentSaveReason> waitUntilReasons;
};

class PassiveRuntimeDocumentUpdateHandler final : public DocumentUpdateHandler {
public:
  bool supportsDidChangeWatchedFiles() const noexcept override { return true; }

  void didOpenDocument(const TextDocumentChangeEvent &event) override {
    openedUris.push_back(event.document->uri);
  }

  void didChangeContent(const TextDocumentChangeEvent &event) override {
    changedUris.push_back(event.document->uri);
  }

  void didSaveDocument(const TextDocumentChangeEvent &event) override {
    savedUris.push_back(event.document->uri);
  }

  void didCloseDocument(const TextDocumentChangeEvent &event) override {
    closedUris.push_back(event.document->uri);
  }

  void
  didChangeWatchedFiles(const ::lsp::DidChangeWatchedFilesParams &params) override {
    watchedChangeCounts.push_back(params.changes.size());
  }

  std::vector<std::string> openedUris;
  std::vector<std::string> changedUris;
  std::vector<std::string> savedUris;
  std::vector<std::string> closedUris;
  std::vector<std::size_t> watchedChangeCounts;
};

class RecordingRuntimeFileOperationHandler final : public FileOperationHandler {
public:
  [[nodiscard]] bool supportsDidCreateFiles() const noexcept override {
    return true;
  }

  [[nodiscard]] bool supportsWillDeleteFiles() const noexcept override {
    return true;
  }

  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override {
    return options;
  }

  void didCreateFiles(const ::lsp::CreateFilesParams &params) override {
    didCreateCalls.push_back(params.files.size());
  }

  void didRenameFiles(const ::lsp::RenameFilesParams &params) override {
    didRenameCalls.push_back(params.files.size());
  }

  std::optional<::lsp::WorkspaceEdit>
  willDeleteFiles(const ::lsp::DeleteFilesParams &params) override {
    willDeleteCalls.push_back(params.files.size());
    return ::lsp::WorkspaceEdit{};
  }

  std::vector<std::size_t> didCreateCalls;
  std::vector<std::size_t> didRenameCalls;
  std::vector<std::size_t> willDeleteCalls;

private:
  ::lsp::FileOperationOptions options = [] {
    ::lsp::FileOperationFilter fileFilter{};
    fileFilter.pattern.glob = "**/*";
    fileFilter.scheme = "file";

    ::lsp::FileOperationRegistrationOptions registration{};
    registration.filters.push_back(std::move(fileFilter));

    ::lsp::FileOperationOptions out{};
    out.didCreate = registration;
    out.didRename = registration;
    out.willDelete = registration;
    return out;
  }();
};

class LanguageServerRuntimeTest : public ::testing::Test {
protected:
  std::unique_ptr<services::SharedServices> shared = test::make_shared_services();

  struct MessageHarness {
    test::MemoryStream stream;
    ::lsp::Connection connection;
    ::lsp::MessageHandler handler;
    utils::DisposableStore disposables;

    MessageHarness() : connection(stream), handler(connection) {}
  };

  std::string fileUri(std::string_view fileName) const {
    return test::make_file_uri(fileName);
  }

  std::unique_ptr<MessageHarness> makeHarnessWithDocumentHandler() {
    auto harness = std::make_unique<MessageHarness>();
    addDocumentUpdateHandler(harness->handler, *shared, [] {},
                             harness->disposables);
    return harness;
  }

  std::unique_ptr<MessageHarness>
  makeHarnessWithFileHandler(FileOperationHandler &fileOperationHandler) {
    auto harness = std::make_unique<MessageHarness>();
    addFileOperationHandler(harness->handler, fileOperationHandler, [] {});
    return harness;
  }
};

TEST_F(LanguageServerRuntimeTest,
       AddDocumentUpdateHandlerWiresTextDocumentEventsToUpdateHandler) {
  auto recordingHandler = std::make_unique<RecordingRuntimeDocumentUpdateHandler>();
  auto *recording = recordingHandler.get();
  shared->lsp.documentUpdateHandler = std::move(recordingHandler);

  auto harness = makeHarnessWithDocumentHandler();

  const auto uri = fileUri("runtime-subscriptions.test");
  ASSERT_NE(shared->workspace.textDocuments->open(uri, "test", "alpha", 1),
            nullptr);
  ASSERT_EQ(recording->openedUris.size(), 1u);
  ASSERT_EQ(recording->changedUris.size(), 1u);
  EXPECT_EQ(recording->openedUris.front(), uri);
  EXPECT_EQ(recording->changedUris.front(), uri);

  EXPECT_TRUE(shared->workspace.textDocuments->willSave(
      uri, workspace::TextDocumentSaveReason::FocusOut));
  ASSERT_EQ(recording->willSaveReasons.size(), 1u);
  EXPECT_EQ(recording->willSaveReasons.front(),
            ::lsp::TextDocumentSaveReason::FocusOut);

  const auto edits = shared->workspace.textDocuments->willSaveWaitUntil(
      uri, workspace::TextDocumentSaveReason::AfterDelay);
  ASSERT_EQ(edits.size(), 1u);
  EXPECT_EQ(edits.front().newText, "runtime ");
  ASSERT_EQ(recording->waitUntilReasons.size(), 1u);
  EXPECT_EQ(recording->waitUntilReasons.front(),
            ::lsp::TextDocumentSaveReason::AfterDelay);

  ASSERT_NE(shared->workspace.textDocuments->save(uri, std::string("beta")),
            nullptr);
  ASSERT_EQ(recording->savedUris.size(), 1u);
  EXPECT_EQ(recording->savedUris.front(), uri);

  EXPECT_TRUE(shared->workspace.textDocuments->close(uri));
  ASSERT_EQ(recording->closedUris.size(), 1u);
  EXPECT_EQ(recording->closedUris.front(), uri);
}

TEST_F(LanguageServerRuntimeTest,
       AddFileOperationHandlerRegistersOnlySupportedOperations) {
  RecordingRuntimeFileOperationHandler fileOperationHandler;

  auto harness = makeHarnessWithFileHandler(fileOperationHandler);

  ::lsp::CreateFilesParams didCreate{};
  didCreate.files.push_back(::lsp::FileCreate{
      .uri = fileUri("created.test"),
  });
  harness->stream.pushInput(test::make_notification_message(
      ::lsp::notifications::Workspace_DidCreateFiles::Method, didCreate));
  harness->handler.processIncomingMessages();

  ::lsp::RenameFilesParams didRename{};
  didRename.files.push_back(::lsp::FileRename{
      .oldUri = fileUri("before.test"),
      .newUri = fileUri("after.test"),
  });
  harness->stream.pushInput(test::make_notification_message(
      ::lsp::notifications::Workspace_DidRenameFiles::Method, didRename));
  harness->handler.processIncomingMessages();

  ::lsp::DeleteFilesParams willDelete{};
  willDelete.files.push_back(::lsp::FileDelete{
      .uri = fileUri("deleted.test"),
  });
  harness->stream.pushInput(test::make_request_message(
      1, ::lsp::requests::Workspace_WillDeleteFiles::Method, willDelete));
  harness->handler.processIncomingMessages();

  EXPECT_EQ(fileOperationHandler.didCreateCalls,
            std::vector<std::size_t>{1u});
  EXPECT_TRUE(fileOperationHandler.didRenameCalls.empty());
  EXPECT_EQ(fileOperationHandler.willDeleteCalls,
            std::vector<std::size_t>{1u});
}

TEST_F(LanguageServerRuntimeTest,
       AddDocumentUpdateHandlerOnlyWiresSupportedCallbacks) {
  auto handlerImpl = std::make_unique<PassiveRuntimeDocumentUpdateHandler>();
  auto *recording = handlerImpl.get();
  shared->lsp.documentUpdateHandler = std::move(handlerImpl);

  auto harness = makeHarnessWithDocumentHandler();

  const auto uri = fileUri("runtime-passive.test");
  ASSERT_NE(shared->workspace.textDocuments->open(uri, "test", "alpha", 1),
            nullptr);
  ASSERT_NE(shared->workspace.textDocuments->save(uri, std::string("beta")),
            nullptr);
  EXPECT_TRUE(shared->workspace.textDocuments->close(uri));

  ::lsp::DidChangeWatchedFilesParams params{};
  params.changes.push_back(::lsp::FileEvent{
      .uri = ::lsp::FileUri(::lsp::Uri::parse(fileUri("watched.test"))),
      .type = ::lsp::FileChangeType::Changed,
  });
  harness->stream.pushInput(test::make_notification_message(
      ::lsp::notifications::Workspace_DidChangeWatchedFiles::Method, params));
  harness->handler.processIncomingMessages();

  EXPECT_TRUE(recording->openedUris.empty());
  EXPECT_TRUE(recording->changedUris.empty());
  EXPECT_TRUE(recording->savedUris.empty());
  EXPECT_TRUE(recording->closedUris.empty());
  EXPECT_EQ(recording->watchedChangeCounts, std::vector<std::size_t>{1u});
}

} // namespace
} // namespace pegium::lsp
