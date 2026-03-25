#include <gtest/gtest.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/services/AbstractExecuteCommandHandler.hpp>

namespace pegium {
namespace {

class TestExecuteCommandHandler final : public AbstractExecuteCommandHandler {
public:
  explicit TestExecuteCommandHandler(pegium::SharedServices &sharedServices)
      : AbstractExecuteCommandHandler(sharedServices) {}

protected:
  void registerCommands(const ExecuteCommandAcceptor &acceptor) const override {
    acceptor("test.echo",
             [](const ::lsp::LSPArray &arguments,
                const utils::CancellationToken &) -> std::optional<::lsp::LSPAny> {
               if (arguments.empty()) {
                 return std::nullopt;
               }
               return arguments.front();
             });
    acceptor("test.upper",
             [](const ::lsp::LSPArray &arguments,
                const utils::CancellationToken &) -> std::optional<::lsp::LSPAny> {
               if (arguments.empty() || !arguments.front().isString()) {
                 return std::nullopt;
               }
               auto value = arguments.front().string();
               std::ranges::transform(value, value.begin(), [](unsigned char c) {
                 return static_cast<char>(std::toupper(c));
               });
               return ::lsp::LSPAny(std::move(value));
             });
  }
};

TEST(AbstractExecuteCommandHandlerTest, RegisteredCommandsPreserveRegistrationOrder) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  TestExecuteCommandHandler handler(*shared);

  const auto commands = handler.commands();
  EXPECT_EQ(commands, (std::vector<std::string>{"test.echo", "test.upper"}));
}

TEST(AbstractExecuteCommandHandlerTest, ExecuteCommandReturnsRegisteredResult) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  TestExecuteCommandHandler handler(*shared);

  ::lsp::LSPArray arguments;
  arguments.push_back(::lsp::LSPAny(std::string("hello")));

  const auto result = handler.executeCommand("test.echo", arguments);
  ASSERT_TRUE(result.has_value());
  ASSERT_TRUE(result->isString());
  EXPECT_EQ(result->string(), "hello");
  EXPECT_FALSE(handler.executeCommand("missing", arguments).has_value());
}

} // namespace
} // namespace pegium
