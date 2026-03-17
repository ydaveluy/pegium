#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultFileOperationHandler.hpp>

namespace pegium::lsp {
namespace {

TEST(DefaultFileOperationHandlerTest, DidRenameFilesForwardsDeleteAndCreateEvents) {
  auto shared = test::make_shared_services();
  auto *updates = new test::RecordingDocumentUpdateHandler();
  shared->lsp.documentUpdateHandler.reset(updates);

  DefaultFileOperationHandler handler(*shared);

  ::lsp::RenameFilesParams params{};
  params.files.push_back(::lsp::FileRename{
      .oldUri = test::make_file_uri("old.test"),
      .newUri = test::make_file_uri("new.test"),
  });

  handler.didRenameFiles(params);

  ASSERT_EQ(updates->callCount(), 1u);
  const auto call = updates->lastCall();
  ASSERT_EQ(call.changes.size(), 2u);
  EXPECT_EQ(call.changes[0].type, ::lsp::FileChangeType::Deleted);
  EXPECT_EQ(call.changes[0].uri.toString(), test::make_file_uri("old.test"));
  EXPECT_EQ(call.changes[1].type, ::lsp::FileChangeType::Created);
  EXPECT_EQ(call.changes[1].uri.toString(), test::make_file_uri("new.test"));
}

TEST(DefaultFileOperationHandlerTest, ExposesDidFileOperationRegistrations) {
  auto shared = test::make_shared_services();
  DefaultFileOperationHandler handler(*shared);

  EXPECT_TRUE(handler.supportsDidCreateFiles());
  EXPECT_TRUE(handler.supportsDidRenameFiles());
  EXPECT_TRUE(handler.supportsDidDeleteFiles());
  EXPECT_FALSE(handler.supportsWillCreateFiles());
  EXPECT_FALSE(handler.supportsWillRenameFiles());
  EXPECT_FALSE(handler.supportsWillDeleteFiles());

  const auto &options = handler.fileOperationOptions();
  EXPECT_TRUE(options.didCreate.has_value());
  EXPECT_TRUE(options.didRename.has_value());
  EXPECT_TRUE(options.didDelete.has_value());
}

} // namespace
} // namespace pegium::lsp
