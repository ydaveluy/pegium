#include <gtest/gtest.h>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/LanguageServerRequestHandlers.hpp>

namespace pegium::lsp {
namespace {

TEST(LanguageServerRequestHandlersTest, RegistersHandlersOnlyOncePerContext) {
  auto shared = test::make_shared_services();
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
} // namespace pegium::lsp
