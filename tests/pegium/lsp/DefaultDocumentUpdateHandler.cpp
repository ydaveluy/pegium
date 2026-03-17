#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultDocumentUpdateHandler.hpp>
#include <pegium/workspace/Configuration.hpp>

namespace pegium::lsp {
namespace {

void set_validation_configuration(
    workspace::ConfigurationProvider &configurationProvider,
    std::string_view languageId, bool enabled,
    std::vector<std::string> categories) {
  services::JsonValue::Array categoryValues;
  categoryValues.reserve(categories.size());
  for (auto &category : categories) {
    categoryValues.emplace_back(std::move(category));
  }

  workspace::ConfigurationChangeParams params;
  params.settings = services::JsonValue(services::JsonValue::Object{
      {std::string(languageId),
       services::JsonValue(services::JsonValue::Object{
           {"validation",
            services::JsonValue(services::JsonValue::Object{
                {"enabled", services::JsonValue(enabled)},
                {"categories", services::JsonValue(std::move(categoryValues))},
            })},
       })},
  });
  configurationProvider.updateConfiguration(params);
}

void clear_language_configuration(
    workspace::ConfigurationProvider &configurationProvider,
    std::string_view languageId) {
  workspace::ConfigurationChangeParams params;
  params.settings = services::JsonValue(services::JsonValue::Object{
      {std::string(languageId),
       services::JsonValue(services::JsonValue::Object{})},
  });
  configurationProvider.updateConfiguration(params);
}

class DefaultDocumentUpdateHandlerTest : public ::testing::Test {
protected:
  std::unique_ptr<services::SharedServices> shared = test::make_shared_services();
  test::RecordingDocumentBuilder *builder = nullptr;
  std::unique_ptr<DefaultDocumentUpdateHandler> handler;

  void SetUp() override {
    builder = new test::RecordingDocumentBuilder();
    shared->workspace.documentBuilder.reset(builder);
    handler = std::make_unique<DefaultDocumentUpdateHandler>(*shared);
  }

  std::shared_ptr<workspace::TextDocument>
  makeTextDocument(std::string_view fileName, std::string text,
                   std::string languageId = {},
                   std::optional<std::int64_t> clientVersion = std::nullopt) {
    auto document = std::make_shared<workspace::TextDocument>();
    document->uri = test::make_file_uri(fileName);
    document->languageId = std::move(languageId);
    document->replaceText(std::move(text));
    document->setClientVersion(clientVersion);
    return document;
  }

  std::shared_ptr<workspace::Document>
  addExistingDocument(std::string_view fileName, std::string text,
                      std::string languageId = "test") {
    auto existing = std::make_shared<workspace::Document>();
    existing->uri = test::make_file_uri(fileName);
    existing->languageId = std::move(languageId);
    existing->setText(std::move(text));
    shared->workspace.documents->addDocument(existing);
    return existing;
  }
};

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidOpenDocumentSchedulesDocumentRebuild) {
  auto document = makeTextDocument("opened.test", "content");
  const auto documentId =
      shared->workspace.documents->getOrCreateDocumentId(document->uri);

  handler->didOpenDocument({.document = document});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{documentId});
  EXPECT_TRUE(call.deletedDocumentIds.empty());
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSchedulesDocumentRebuild) {
  auto document = makeTextDocument("changed.test", "content");
  const auto documentId =
      shared->workspace.documents->getOrCreateDocumentId(document->uri);

  handler->didChangeContent({.document = document});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{documentId});
  EXPECT_TRUE(call.deletedDocumentIds.empty());
}

TEST_F(DefaultDocumentUpdateHandlerTest, DidChangeWatchedFilesDeduplicatesUris) {
  const auto changedUri = test::make_file_uri("one.test");
  const auto deletedUri = test::make_file_uri("gone.test");
  const auto changedDocumentId =
      shared->workspace.documents->getOrCreateDocumentId(changedUri);
  const auto deletedDocumentId =
      shared->workspace.documents->getOrCreateDocumentId(deletedUri);

  ::lsp::DidChangeWatchedFilesParams params{};
  params.changes = {
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(changedUri)),
          .type = ::lsp::FileChangeType::Changed,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(changedUri)),
          .type = ::lsp::FileChangeType::Changed,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(deletedUri)),
          .type = ::lsp::FileChangeType::Deleted,
      },
      ::lsp::FileEvent{
          .uri = ::lsp::FileUri(::lsp::Uri::parse(deletedUri)),
          .type = ::lsp::FileChangeType::Deleted,
      },
  };

  handler->didChangeWatchedFiles(params);

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_EQ(call.changedDocumentIds,
            std::vector<workspace::DocumentId>{changedDocumentId});
  EXPECT_EQ(call.deletedDocumentIds,
            std::vector<workspace::DocumentId>{deletedDocumentId});
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentSkipsRedundantSnapshotForExistingDocument) {
  auto existing = addExistingDocument("redundant.test", "content");
  auto textDocument = makeTextDocument("redundant.test", existing->text(),
                                       existing->languageId, 7);

  handler->didChangeContent({.document = textDocument});

  EXPECT_FALSE(builder->waitForCalls(1, std::chrono::milliseconds(50)));
}

TEST_F(
    DefaultDocumentUpdateHandlerTest,
    DidChangeContentPreservesBuilderDefaultsWithoutValidationConfiguration) {
  builder->updateBuildOptions().validation.enabled = true;
  builder->updateBuildOptions().validation.categories = {"built-in", "fast"};

  auto existing = addExistingDocument("default-options.test", "old");
  auto textDocument =
      makeTextDocument("default-options.test", "new", existing->languageId);

  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_TRUE(call.options.validation.enabled);
  EXPECT_EQ(call.options.validation.categories,
            std::vector<std::string>({"built-in", "fast"}));
  EXPECT_TRUE(builder->updateBuildOptions().validation.enabled);
  EXPECT_EQ(builder->updateBuildOptions().validation.categories,
            std::vector<std::string>({"built-in", "fast"}));
}

TEST_F(DefaultDocumentUpdateHandlerTest,
       DidChangeContentUsesExplicitValidationConfigurationTemporarily) {
  builder->updateBuildOptions().validation.enabled = true;
  builder->updateBuildOptions().validation.categories = {"built-in", "fast"};

  set_validation_configuration(*shared->workspace.configurationProvider, "test",
                               true, {"slow"});

  auto existing = addExistingDocument("configured-options.test", "old");
  auto textDocument =
      makeTextDocument("configured-options.test", "new", existing->languageId);

  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(1));
  const auto call = builder->lastCall();
  EXPECT_TRUE(call.options.validation.enabled);
  EXPECT_EQ(call.options.validation.categories,
            std::vector<std::string>({"slow"}));

  clear_language_configuration(*shared->workspace.configurationProvider, "test");

  textDocument->replaceText("newer");
  handler->didChangeContent({.document = textDocument});

  ASSERT_TRUE(builder->waitForCalls(2));
  const auto restoredCall = builder->lastCall();
  EXPECT_TRUE(restoredCall.options.validation.enabled);
  EXPECT_EQ(restoredCall.options.validation.categories,
            std::vector<std::string>({"built-in", "fast"}));
}

} // namespace
} // namespace pegium::lsp
