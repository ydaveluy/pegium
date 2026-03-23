#include <gtest/gtest.h>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlers.hpp>

namespace pegium {
namespace {

TEST(LanguageServerRequestHandlersTest, RegistersHandlersOnlyOncePerContext) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  DefaultLanguageServer server(*shared);
  LanguageServerRuntimeState runtimeState;
  LanguageServerHandlerContext context(server, *shared, runtimeState);

  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  EXPECT_FALSE(context.handlersRegistered());
  addLanguageServerRequestHandlers(context, {}, handler);
  EXPECT_TRUE(context.handlersRegistered());

  addLanguageServerRequestHandlers(context, {}, handler);
  EXPECT_TRUE(context.handlersRegistered());
}

} // namespace
} // namespace pegium
