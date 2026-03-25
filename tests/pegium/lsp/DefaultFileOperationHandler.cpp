#include <gtest/gtest.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/workspace/DefaultFileOperationHandler.hpp>

namespace pegium {
namespace {

TEST(DefaultFileOperationHandlerTest, DidRenameFilesForwardsDeleteAndCreateEvents) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
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
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  DefaultFileOperationHandler handler(*shared);

  const auto &options = handler.fileOperationOptions();
  EXPECT_TRUE(options.didCreate.has_value());
  EXPECT_TRUE(options.didRename.has_value());
  EXPECT_TRUE(options.didDelete.has_value());
  EXPECT_FALSE(options.willCreate.has_value());
  EXPECT_FALSE(options.willRename.has_value());
  EXPECT_FALSE(options.willDelete.has_value());
}

} // namespace
} // namespace pegium
