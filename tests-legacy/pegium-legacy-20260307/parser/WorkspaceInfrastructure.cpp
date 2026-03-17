#include <gtest/gtest.h>

#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <future>
#include <optional>
#include <string>
#include <thread>

#include <lsp/connection.h>
#include <lsp/io/stream.h>
#include <lsp/messagehandler.h>
#include <lsp/types.h>

#include <pegium/lsp/DefaultDocumentUpdateHandler.hpp>
#include <pegium/lsp/DefaultFileOperationHandler.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/workspace/DefaultTextDocuments.hpp>
#include <pegium/lsp/DefaultUpdateScheduler.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/services/DefaultServiceRegistry.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/Configuration.hpp>
#include <pegium/workspace/DefaultWorkspaceConfigurationProvider.hpp>
#include <pegium/workspace/DefaultIndexManager.hpp>
#include <pegium/workspace/DefaultWorkspaceLock.hpp>
#include <pegium/workspace/Document.hpp>
#include <pegium/workspace/WorkspaceLock.hpp>

namespace {

std::shared_ptr<pegium::workspace::Document>
open_or_update(pegium::services::SharedServices &shared, std::string uri,
               std::string text, std::string languageId) {
  return shared.workspace.documents->openOrUpdate(std::move(uri), std::move(text),
                                                  std::move(languageId));
}

struct AcceptingParser final : pegium::parser::Parser {
  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken & = {}) const override {
    pegium::parser::ParseResult result;
    if (!document.text().empty()) {
      result.value = std::make_unique<pegium::AstNode>();
      result.fullMatch = true;
    }
    document.parseResult = std::move(result);
  }
};

struct KeywordParser final : pegium::parser::Parser {
  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken & = {}) const override {
    pegium::parser::ParseResult result;
    result.parsedLength = static_cast<pegium::TextOffset>(document.text().size());
    if (document.text() == "ok") {
      result.value = std::make_unique<pegium::AstNode>();
      result.fullMatch = true;
    } else {
      result.parseDiagnostics.push_back(
          {.kind = pegium::parser::ParseDiagnosticKind::Deleted, .offset = 0});
    }
    document.parseResult = std::move(result);
  }
};

struct CapturingStream final : lsp::io::Stream {
  std::string written;

  void read(char *buffer, std::size_t size) override {
    for (std::size_t index = 0; index < size; ++index) {
      buffer[index] = Eof;
    }
  }

  void write(const char *buffer, std::size_t size) override {
    written.append(buffer, size);
  }
};

struct RecordingParser final : pegium::parser::Parser {
  mutable std::atomic<int> calls{0};
  mutable std::string lastText;
  mutable std::mutex mutex;

  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken &) const override {
    {
      std::scoped_lock lock(mutex);
      lastText = document.text();
    }
    ++calls;
    pegium::parser::ParseResult result;
    result.value = std::make_unique<pegium::AstNode>();
    result.fullMatch = true;
    document.parseResult = std::move(result);
  }
};

struct CancellableParser final : pegium::parser::Parser {
  mutable std::promise<void> firstStartedPromise;
  mutable std::shared_future<void> firstStarted =
      firstStartedPromise.get_future().share();
  mutable std::atomic<bool> firstStartedSet{false};
  mutable std::atomic<bool> firstCancelled{false};
  mutable std::atomic<int> calls{0};

  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken &cancelToken) const override {
    ++calls;
    if (document.clientVersion() == 1) {
      if (!firstStartedSet.exchange(true)) {
        firstStartedPromise.set_value();
      }
      while (!cancelToken.stop_requested()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      try {
        pegium::utils::throw_if_cancelled(cancelToken);
      } catch (const pegium::utils::OperationCancelled &) {
        firstCancelled.store(true);
        throw;
      }
    }

    pegium::parser::ParseResult result;
    result.value = std::make_unique<pegium::AstNode>();
    result.fullMatch = true;
    document.parseResult = std::move(result);
  }
};

TEST(WorkspaceInfrastructureTest, WorkspaceLockSupportsReadAndWriteActions) {
  pegium::workspace::DefaultWorkspaceLock lock;
  std::atomic<bool> readRan = false;
  std::atomic<bool> writeRan = false;

  auto readFuture = lock.read([&readRan]() { readRan.store(true); });
  auto writeFuture = lock.write([&writeRan](
                                    const pegium::utils::CancellationToken &token) {
    pegium::utils::throw_if_cancelled(token);
    writeRan.store(true);
  });

  ASSERT_NO_THROW(readFuture.get());
  ASSERT_NO_THROW(writeFuture.get());
  EXPECT_TRUE(readRan.load());
  EXPECT_TRUE(writeRan.load());
}

TEST(WorkspaceInfrastructureTest, WorkspaceLockCancelWriteCancelsRunningAction) {
  pegium::workspace::DefaultWorkspaceLock lock;
  std::atomic<bool> started = false;
  std::atomic<bool> cancelled = false;

  auto writeFuture = lock.write([&](const pegium::utils::CancellationToken &token) {
    started.store(true);
    while (!token.stop_requested()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    try {
      pegium::utils::throw_if_cancelled(token);
    } catch (const pegium::utils::OperationCancelled &) {
      cancelled.store(true);
      throw;
    }
  });

  std::this_thread::sleep_for(std::chrono::milliseconds(5));
  lock.cancelWrite();

  ASSERT_NO_THROW(writeFuture.get());
  EXPECT_TRUE(started.load());
  EXPECT_TRUE(cancelled.load());
}

TEST(WorkspaceInfrastructureTest,
     RunWithWorkspaceWriteThrowsWhenCancelledBeforeReturningValue) {
  pegium::workspace::DefaultWorkspaceLock lock;
  pegium::utils::CancellationTokenSource cancellation;
  std::promise<void> startedPromise;
  auto started = startedPromise.get_future();

  auto future = std::async(std::launch::async, [&]() {
    return pegium::workspace::run_with_workspace_write(
        &lock, cancellation.get_token(), [&]() -> int {
          startedPromise.set_value();
          while (!cancellation.get_token().stop_requested()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          return 42;
        });
  });

  ASSERT_EQ(started.wait_for(std::chrono::seconds(1)), std::future_status::ready);
  cancellation.request_stop();

  EXPECT_THROW((void)future.get(), pegium::utils::OperationCancelled);
}

TEST(WorkspaceInfrastructureTest, WorkspaceLockSupportsQueuedReadAndWriteActions) {
  pegium::workspace::DefaultWorkspaceLock lock;
  std::atomic<int> order = 0;

  auto writeFuture = lock.write([&order](const pegium::utils::CancellationToken &) {
    EXPECT_EQ(order.load(), 0);
    order.store(1);
  });
  auto readFuture = lock.read([&order]() {
    EXPECT_EQ(order.load(), 1);
    order.store(2);
  });

  ASSERT_NO_THROW(writeFuture.get());
  ASSERT_NO_THROW(readFuture.get());
  EXPECT_EQ(order.load(), 2);
}

TEST(WorkspaceInfrastructureTest, WorkspaceLockCancelsPreviousWriteAction) {
  pegium::workspace::DefaultWorkspaceLock lock;

  std::atomic<bool> firstStarted = false;
  std::atomic<bool> firstCancelled = false;
  std::atomic<bool> secondRan = false;

  auto first = lock.write([&firstStarted, &firstCancelled](const pegium::utils::CancellationToken &token) {
    firstStarted.store(true);
    for (int i = 0; i < 200; ++i) {
      if (token.stop_requested()) {
        try {
          pegium::utils::throw_if_cancelled(token);
        } catch (const pegium::utils::OperationCancelled &) {
          firstCancelled.store(true);
          throw;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  });

  auto second = lock.write([&secondRan](const pegium::utils::CancellationToken &) {
    secondRan.store(true);
  });

  ASSERT_NO_THROW(first.get());
  ASSERT_NO_THROW(second.get());
  EXPECT_TRUE(firstCancelled.load() || !firstStarted.load());
  EXPECT_TRUE(secondRan.load());
}

TEST(WorkspaceInfrastructureTest, StaticConfigurationProviderReturnsConfiguredValue) {
  pegium::workspace::WorkspaceConfiguration configuration;
  configuration.validation.enabled = false;

  pegium::workspace::StaticWorkspaceConfigurationProvider provider(configuration);
  const auto result = provider.getConfiguration("file:///workspace");

  EXPECT_FALSE(result.validation.enabled);
}

TEST(WorkspaceInfrastructureTest, DefaultConfigurationProviderUpdatesFromLspSettings) {
  pegium::services::SharedServices shared;
  auto language = pegium::services::makeDefaultServices(
      shared, "mini", std::make_unique<AcceptingParser>());
  language->languageMetaData.fileExtensions = {".pg"};
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));

  auto *provider = dynamic_cast<pegium::workspace::DefaultWorkspaceConfigurationProvider *>(
      shared.workspace.configurationProvider.get());
  ASSERT_NE(provider, nullptr);

  EXPECT_FALSE(provider->isReady());
  provider->initialize(::lsp::InitializeParams{});
  ASSERT_NO_THROW(provider->initialized(::lsp::InitializedParams{}).get());
  EXPECT_TRUE(provider->isReady());

  std::size_t updates = 0;
  auto subscription =
      provider->onConfigurationSectionUpdate([&updates](const auto &update) {
        EXPECT_EQ(update.section, "mini");
        ++updates;
      });

  ::lsp::LSPObject validation;
  validation["enabled"] = ::lsp::LSPAny(false);
  ::lsp::LSPArray categories;
  categories.push_back(::lsp::LSPAny(std::string("fast")));
  validation["categories"] = ::lsp::LSPAny(std::move(categories));

  ::lsp::LSPObject section;
  section["validation"] = ::lsp::LSPAny(std::move(validation));

  ::lsp::DidChangeConfigurationParams params{};
  ::lsp::LSPObject settings;
  settings["mini"] = ::lsp::LSPAny(std::move(section));
  params.settings = ::lsp::LSPAny(std::move(settings));
  provider->updateConfiguration(params);

  const auto configuration = provider->getConfiguration("file:///tmp/model.pg");
  EXPECT_FALSE(configuration.validation.enabled);
  ASSERT_EQ(configuration.validation.categories.size(), 1u);
  EXPECT_EQ(configuration.validation.categories.front(), "fast");

  const auto value = provider->getConfigurationValue("mini", "validation");
  ASSERT_TRUE(value.has_value());
  EXPECT_TRUE(value->isObject());
  EXPECT_EQ(updates, 1u);
  (void)subscription;
}

TEST(WorkspaceInfrastructureTest,
     WorkspaceManagerInitializationIndexesWorkspaceFolderFiles) {
  pegium::services::SharedServices shared;
  auto language = pegium::services::makeDefaultServices(
      shared, "mini", std::make_unique<AcceptingParser>());
  language->languageMetaData.fileExtensions = {".pg"};
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));

  const auto tempDir = std::filesystem::temp_directory_path() /
                       ("pegium-workspace-init-" +
                        std::to_string(std::chrono::steady_clock::now()
                                           .time_since_epoch()
                                           .count()));
  ASSERT_TRUE(std::filesystem::create_directories(tempDir));
  const auto visibleFile = tempDir / "visible.pg";
  const auto hiddenFile = tempDir / ".hidden.pg";
  {
    std::ofstream stream(visibleFile, std::ios::binary);
    ASSERT_TRUE(stream.is_open());
    stream << "visible";
  }
  {
    std::ofstream stream(hiddenFile, std::ios::binary);
    ASSERT_TRUE(stream.is_open());
    stream << "hidden";
  }

  ::lsp::InitializeParams params{};
  ::lsp::WorkspaceFolder folder{};
  folder.uri = ::lsp::Uri::parse(pegium::utils::path_to_file_uri(tempDir.string()));
  folder.name = "workspace";
  ::lsp::Array<::lsp::WorkspaceFolder> folders;
  folders.push_back(std::move(folder));
  params.workspaceFolders = ::lsp::NullOr<::lsp::Array<::lsp::WorkspaceFolder>>(
      std::move(folders));

  shared.workspace.workspaceManager->initialize(params);
  ASSERT_FALSE(shared.workspace.workspaceManager->isReady());
  ASSERT_NO_THROW(
      shared.workspace.workspaceManager->initialized(::lsp::InitializedParams{}).get());
  EXPECT_TRUE(shared.workspace.workspaceManager->isReady());

  const auto visibleUri = pegium::utils::path_to_file_uri(visibleFile.string());
  const auto hiddenUri = pegium::utils::path_to_file_uri(hiddenFile.string());
  EXPECT_NE(shared.workspace.documents->getDocument(visibleUri), nullptr);
  EXPECT_EQ(shared.workspace.documents->getDocument(hiddenUri), nullptr);

  std::error_code ec;
  std::filesystem::remove_all(tempDir, ec);
}

TEST(WorkspaceInfrastructureTest, TextDocumentsAppliesFullAndRangeChanges) {
  pegium::workspace::DefaultTextDocuments documents;
  ASSERT_NE(documents.open("file:///doc.pg", "mini", "abcdef"), nullptr);

  auto full = documents.replaceText("file:///doc.pg", "hello", "mini");
  ASSERT_NE(full, nullptr);
  ASSERT_NE(documents.get("file:///doc.pg"), nullptr);
  EXPECT_EQ(documents.get("file:///doc.pg")->text(), "hello");

  const auto changes = std::array{
      pegium::workspace::TextDocumentContentChange{
          .range = pegium::workspace::TextDocumentContentChangeRange{
              .start = pegium::Position{0, 1},
              .end = pegium::Position{0, 4}},
          .text = "XX"}};
  const auto updated =
      documents.applyContentChanges("file:///doc.pg", changes);
  ASSERT_NE(updated, nullptr);
  EXPECT_EQ(updated->text(), "hXXo");

  EXPECT_TRUE(documents.close("file:///doc.pg"));
  EXPECT_EQ(documents.get("file:///doc.pg"), nullptr);
}

TEST(WorkspaceInfrastructureTest, TextDocumentsRangeUsesUtf16Columns) {
  pegium::workspace::DefaultTextDocuments documents;
  ASSERT_NE(documents.open("file:///doc.pg", "mini",
                           "a" "\xF0\x9F\x98\x80" "b"),
            nullptr);

  const auto changes = std::array{
      pegium::workspace::TextDocumentContentChange{
          .range = pegium::workspace::TextDocumentContentChangeRange{
              .start = pegium::Position{0, 1},
              .end = pegium::Position{0, 3}},
          .text = "X"}};
  const auto updated =
      documents.applyContentChanges("file:///doc.pg", changes);
  ASSERT_NE(updated, nullptr);
  EXPECT_EQ(updated->text(), "aXb");
}

TEST(WorkspaceInfrastructureTest, DocumentPositionMappingUsesUtf16Columns) {
  pegium::workspace::Document document;
  document.setText("a" "\xF0\x9F\x98\x80" "b\n");

  EXPECT_EQ(document.positionToOffset({.line = 0, .character = 0}), 0u);
  EXPECT_EQ(document.positionToOffset({.line = 0, .character = 1}), 1u);
  EXPECT_EQ(document.positionToOffset({.line = 0, .character = 3}), 5u);
  EXPECT_EQ(document.positionToOffset({.line = 0, .character = 4}), 6u);

  EXPECT_EQ(document.offsetToPosition(1).character, 1u);
  EXPECT_EQ(document.offsetToPosition(5).character, 3u);
  EXPECT_EQ(document.offsetToPosition(6).character, 4u);
}

TEST(WorkspaceInfrastructureTest, DocumentPositionMappingRoundTripsUtf16Columns) {
  pegium::workspace::Document document;
  document.setText("ab\n" "\xF0\x9F\x98\x80" "z\nlast");

  const std::vector<pegium::Position> positions = {
      {.line = 0, .character = 0},
      {.line = 0, .character = 2},
      {.line = 1, .character = 0},
      {.line = 1, .character = 2},
      {.line = 1, .character = 3},
      {.line = 2, .character = 0},
      {.line = 2, .character = 4},
  };

  for (const auto &position : positions) {
    const auto offset = document.positionToOffset(position);
    const auto roundTrip = document.offsetToPosition(offset);
    EXPECT_EQ(roundTrip.line, position.line);
    EXPECT_EQ(roundTrip.character, position.character);
  }
}

TEST(WorkspaceInfrastructureTest, DocumentLineIndexSupportsConcurrentReaders) {
  pegium::workspace::Document document;
  document.setText("ab\n" "\xF0\x9F\x98\x80" "z\nlast line\n");

  auto worker = [&document]() {
    for (int i = 0; i < 2000; ++i) {
      EXPECT_EQ(document.positionToOffset({.line = 0, .character = 2}), 2u);
      EXPECT_EQ(document.positionToOffset({.line = 1, .character = 2}), 7u);
      const auto first = document.offsetToPosition(0);
      EXPECT_EQ(first.line, 0u);
      EXPECT_EQ(first.character, 0u);
      const auto emojiEnd = document.offsetToPosition(7);
      EXPECT_EQ(emojiEnd.line, 1u);
      EXPECT_EQ(emojiEnd.character, 2u);
      const auto last = document.offsetToPosition(13);
      EXPECT_EQ(last.line, 2u);
      EXPECT_EQ(last.character, 4u);
    }
  };

  auto one = std::async(std::launch::async, worker);
  auto two = std::async(std::launch::async, worker);
  auto three = std::async(std::launch::async, worker);
  auto four = std::async(std::launch::async, worker);

  ASSERT_NO_THROW(one.get());
  ASSERT_NO_THROW(two.get());
  ASSERT_NO_THROW(three.get());
  ASSERT_NO_THROW(four.get());
}

TEST(WorkspaceInfrastructureTest, DocumentSetTextResetsPositionIndex) {
  pegium::workspace::Document document;
  document.setText("a\nb");
  EXPECT_EQ(document.positionToOffset({.line = 1, .character = 0}), 2u);

  document.setText("x\ny\nz");
  EXPECT_EQ(document.positionToOffset({.line = 2, .character = 0}), 4u);
}

TEST(WorkspaceInfrastructureTest, FileOperationHandlerAppliesRenameAndDelete) {
  pegium::services::SharedServices shared;
  pegium::lsp::DefaultUpdateScheduler scheduler;
  pegium::lsp::DefaultFileOperationHandler handler(shared, scheduler);

  constexpr std::string_view oldUri = "file:///old.pg";
  constexpr std::string_view newUri = "file:///new.pg";

  (void)open_or_update(shared, std::string(oldUri), "content", "mini");

  ::lsp::RenameFilesParams renameParams{};
  ::lsp::FileRename rename{};
  rename.oldUri = std::string(oldUri);
  rename.newUri = std::string(newUri);
  renameParams.files.push_back(std::move(rename));

  ASSERT_NO_THROW(handler.didRenameFiles(renameParams).get());
  EXPECT_EQ(shared.workspace.documents->getDocument(oldUri), nullptr);
  ASSERT_NE(shared.workspace.documents->getDocument(newUri), nullptr);

  ::lsp::DeleteFilesParams deleteParams{};
  ::lsp::FileDelete deletion{};
  deletion.uri = std::string(newUri);
  deleteParams.files.push_back(std::move(deletion));

  ASSERT_NO_THROW(handler.didDeleteFiles(deleteParams).get());
  EXPECT_EQ(shared.workspace.documents->getDocument(newUri), nullptr);
}

TEST(WorkspaceInfrastructureTest, FileOperationHandlerProvidesCapabilities) {
  pegium::services::SharedServices shared;
  pegium::lsp::DefaultUpdateScheduler scheduler;
  pegium::lsp::DefaultFileOperationHandler handler(shared, scheduler);

  const auto &options = handler.fileOperationOptions();
  EXPECT_TRUE(options.didCreate.has_value());
  EXPECT_FALSE(options.willCreate.has_value());
  EXPECT_TRUE(options.didRename.has_value());
  EXPECT_FALSE(options.willRename.has_value());
  EXPECT_TRUE(options.didDelete.has_value());
  EXPECT_FALSE(options.willDelete.has_value());
}

TEST(WorkspaceInfrastructureTest, IndexManagerExposesStreamContracts) {
  pegium::workspace::DefaultIndexManager index;
  index.setExports("file:///types.pg",
                   {{.name = "Entity",
                     .type = "type",
                     .documentUri = "file:///types.pg",
                     .offset = 1,
                     .path = "0"}});
  index.setReferences("file:///refs.pg",
                      {{.sourceUri = "file:///refs.pg",
                        .sourcePath = "1",
                        .sourceOffset = 2,
                        .sourceLength = 6,
                        .referenceType = "type",
                        .targetName = "Entity",
                        .targetUri = std::string("file:///types.pg"),
                        .targetPath = std::string("0")}});

  std::size_t elementCount = 0;
  for (const auto &element : index.allElements("type")) {
    EXPECT_EQ(element.name, "Entity");
    ++elementCount;
  }
  EXPECT_EQ(elementCount, 1u);

  std::size_t referenceCount = 0;
  for (const auto &entry :
       index.findAllReferences({.uri = "file:///types.pg", .path = "0"}, true)) {
    (void)entry;
    ++referenceCount;
  }
  EXPECT_EQ(referenceCount, 2u);
}

TEST(WorkspaceInfrastructureTest, ServiceRegistryResolvesServicesFromUriMetadata) {
  pegium::services::DefaultServiceRegistry registry;
  pegium::services::SharedServices shared;

  auto one = pegium::services::makeDefaultServices(
      shared, "lang-one", std::make_unique<AcceptingParser>());
  one->languageMetaData.fileExtensions = {".one"};
  one->languageMetaData.fileNames = {"LANG.ONE.SPEC"};
  ASSERT_TRUE(registry.registerLanguage(std::move(one)));

  auto two = pegium::services::makeDefaultServices(
      shared, "lang-two", std::make_unique<AcceptingParser>());
  two->languageMetaData.fileExtensions = {".two"};
  ASSERT_TRUE(registry.registerLanguage(std::move(two)));

  const auto *byExtension = registry.getServicesForUri("file:///tmp/file.one");
  ASSERT_NE(byExtension, nullptr);
  EXPECT_EQ(byExtension->languageId, "lang-one");

  const auto *byFileName =
      registry.getServicesForUri("file:///tmp/lang.one.spec");
  ASSERT_NE(byFileName, nullptr);
  EXPECT_EQ(byFileName->languageId, "lang-one");

  const auto *byOtherExtension =
      registry.getServicesForFileName("/tmp/another.two");
  ASSERT_NE(byOtherExtension, nullptr);
  EXPECT_EQ(byOtherExtension->languageId, "lang-two");
}

TEST(WorkspaceInfrastructureTest,
     FileOperationHandlerCreateLoadsFileAndInfersLanguageFromExtension) {
  pegium::services::SharedServices shared;
  auto language = pegium::services::makeDefaultServices(
      shared, "mini", std::make_unique<AcceptingParser>());
  language->languageMetaData.fileExtensions = {".pg"};
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));
  pegium::lsp::DefaultUpdateScheduler scheduler;
  pegium::lsp::DefaultFileOperationHandler handler(shared, scheduler);

  const auto tempFile = std::filesystem::temp_directory_path() /
                        ("pegium-create-" +
                         std::to_string(std::chrono::steady_clock::now()
                                            .time_since_epoch()
                                            .count()) +
                         ".pg");
  {
    std::ofstream stream(tempFile, std::ios::binary);
    ASSERT_TRUE(stream.is_open());
    stream << "created-content";
  }

  const auto createdUri = pegium::utils::path_to_file_uri(tempFile.string());

  ::lsp::CreateFilesParams createParams{};
  ::lsp::FileCreate create{};
  create.uri = createdUri;
  createParams.files.push_back(std::move(create));

  ASSERT_NO_THROW(handler.didCreateFiles(createParams).get());

  auto created = shared.workspace.documents->getDocument(createdUri);
  ASSERT_NE(created, nullptr);
  EXPECT_EQ(created->languageId, "mini");
  EXPECT_EQ(created->text(), "created-content");

  std::error_code ec;
  std::filesystem::remove(tempFile, ec);
}

TEST(WorkspaceInfrastructureTest,
     DocumentUpdateHandlerDidSaveUsesCurrentWorkspaceContent) {
  pegium::services::SharedServices shared;
  auto parser = std::make_unique<AcceptingParser>();
  auto language = pegium::services::makeDefaultServices(shared, "mini", std::move(parser));
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));

  pegium::lsp::DefaultDocumentUpdateHandler handler(shared);

  ASSERT_NO_THROW(handler.didOpen("file:///save.pg", "mini", "initial").get());
  (void)open_or_update(shared, "file:///save.pg", "updated", "mini");

  ASSERT_NO_THROW(handler.didSave("file:///save.pg").get());

  auto document = shared.workspace.documents->getDocument("file:///save.pg");
  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(document->parseSucceeded());
  EXPECT_EQ(document->text(), "updated");
  ASSERT_NE(shared.workspace.textDocuments, nullptr);
  const auto textDocument =
      shared.workspace.textDocuments->get("file:///save.pg");
  ASSERT_NE(textDocument, nullptr);
  EXPECT_EQ(textDocument->text(), "updated");
}

TEST(WorkspaceInfrastructureTest,
     DocumentUpdateHandlerKeepsLatestDidChangeText) {
  pegium::services::SharedServices shared;
  auto parser = std::make_unique<RecordingParser>();
  auto *parserPtr = parser.get();
  auto language =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parser));
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));

  pegium::lsp::DefaultDocumentUpdateHandler handler(shared);

  ASSERT_NO_THROW(handler.didOpen("file:///debounce.pg", "mini", "a").get());
  parserPtr->calls.store(0);

  ::lsp::TextDocumentContentChangeEvent_Text first{};
  first.text = "ab";
  auto firstFuture = handler.didChange("file:///debounce.pg", 1, {first});

  ::lsp::TextDocumentContentChangeEvent_Text second{};
  second.text = "abc";
  auto secondFuture = handler.didChange("file:///debounce.pg", 2, {second});

  ASSERT_NO_THROW(firstFuture.get());
  ASSERT_NO_THROW(secondFuture.get());

  EXPECT_GE(parserPtr->calls.load(), 1);
  std::scoped_lock lock(parserPtr->mutex);
  EXPECT_EQ(parserPtr->lastText, "abc");
}

TEST(WorkspaceInfrastructureTest,
     DocumentUpdateHandlerPropagatesDidChangeCancellationToServer) {
  pegium::services::SharedServices shared;
  auto parser = std::make_unique<CancellableParser>();
  auto *parserPtr = parser.get();
  auto language =
      pegium::services::makeDefaultServices(shared, "mini", std::move(parser));
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));

  pegium::lsp::DefaultDocumentUpdateHandler handler(shared);

  ASSERT_NO_THROW(handler.didOpen("file:///cancel.pg", "mini", "a").get());

  ::lsp::TextDocumentContentChangeEvent_Text first{};
  first.text = "ab";
  auto firstFuture = handler.didChange("file:///cancel.pg", 1, {first});
  ASSERT_EQ(parserPtr->firstStarted.wait_for(std::chrono::seconds(2)),
            std::future_status::ready);

  ::lsp::TextDocumentContentChangeEvent_Text second{};
  second.text = "abc";
  auto secondFuture = handler.didChange("file:///cancel.pg", 2, {second});

  ASSERT_NO_THROW(firstFuture.get());
  ASSERT_NO_THROW(secondFuture.get());
  EXPECT_TRUE(parserPtr->firstCancelled.load());
  EXPECT_GE(parserPtr->calls.load(), 2);
}

TEST(WorkspaceInfrastructureTest,
     DocumentUpdateHandlerWatchedFilesSplitsChangedAndDeletedUris) {
  pegium::services::SharedServices shared;
  pegium::lsp::DefaultDocumentUpdateHandler handler(shared);

  (void)open_or_update(shared, "file:///changed.pg", "x", "mini");
  (void)open_or_update(shared, "file:///deleted.pg", "y", "mini");

  ::lsp::DidChangeWatchedFilesParams params{};
  ::lsp::FileEvent changed{};
  changed.uri = ::lsp::Uri::parse("file:///changed.pg");
  changed.type = ::lsp::FileChangeType::Changed;
  params.changes.push_back(changed);
  params.changes.push_back(changed);

  ::lsp::FileEvent deleted{};
  deleted.uri = ::lsp::Uri::parse("file:///deleted.pg");
  deleted.type = ::lsp::FileChangeType::Deleted;
  params.changes.push_back(deleted);

  ASSERT_NO_THROW(handler.didChangeWatchedFiles(params).get());
  EXPECT_NE(shared.workspace.documents->getDocument("file:///changed.pg"), nullptr);
  EXPECT_EQ(shared.workspace.documents->getDocument("file:///deleted.pg"), nullptr);
}

TEST(WorkspaceInfrastructureTest,
     DocumentUpdateHandlerEmitsWatchedFilesChangeEvents) {
  pegium::services::SharedServices shared;
  pegium::lsp::DefaultDocumentUpdateHandler handler(shared);

  std::size_t eventCount = 0;
  auto listener = handler.onWatchedFilesChange(
      [&eventCount](const ::lsp::DidChangeWatchedFilesParams &) {
        ++eventCount;
      });

  ::lsp::DidChangeWatchedFilesParams params{};
  ::lsp::FileEvent changed{};
  changed.uri = ::lsp::Uri::parse("file:///event.pg");
  changed.type = ::lsp::FileChangeType::Changed;
  params.changes.push_back(changed);

  ASSERT_NO_THROW(handler.didChangeWatchedFiles(params).get());
  EXPECT_EQ(eventCount, 1u);

  listener.dispose();
  ASSERT_NO_THROW(handler.didChangeWatchedFiles(params).get());
  EXPECT_EQ(eventCount, 1u);
}

} // namespace
