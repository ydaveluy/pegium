#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/ExecuteCommandHandler.hpp>
#include <pegium/lsp/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>

namespace pegium::lsp {
namespace {

class TestExecuteCommandHandler final : public ExecuteCommandHandler {
public:
  enum class Mode : std::uint8_t {
    ReturnsValue,
    ReturnsNull,
  };

  explicit TestExecuteCommandHandler(Mode mode = Mode::ReturnsValue)
      : mode(mode) {}

  [[nodiscard]] std::vector<std::string> commands() const override {
    return {"pegium.testCommand"};
  }

  std::optional<::lsp::LSPAny>
  executeCommand(std::string_view name, const ::lsp::LSPArray &arguments,
                 const utils::CancellationToken &) const override {
    lastCommand = std::string(name);
    lastArgumentCount = arguments.size();

    if (mode == Mode::ReturnsNull) {
      return std::nullopt;
    }

    ::lsp::LSPObject result;
    result["handled"] = true;
    result["argumentCount"] = static_cast<int>(arguments.size());
    return result;
  }

  Mode mode;
  mutable std::string lastCommand;
  mutable std::size_t lastArgumentCount = 0;
};

class TestWorkspaceSymbolProvider final : public services::WorkspaceSymbolProvider {
public:
  std::vector<::lsp::WorkspaceSymbol>
  getSymbols(const ::lsp::WorkspaceSymbolParams &params,
             const utils::CancellationToken &) const override {
    lastQuery = params.query;

    ::lsp::WorkspaceSymbol symbol{};
    symbol.name = "value";
    symbol.kind = ::lsp::SymbolKind::Variable;
    ::lsp::Location location{};
    location.uri = ::lsp::DocumentUri(::lsp::Uri::parse(
        test::make_file_uri("workspace-symbol.test")));
    location.range.start = text::Position(1, 1);
    location.range.end = text::Position(1, 6);
    symbol.location = std::move(location);
    return {std::move(symbol)};
  }
  mutable std::string lastQuery;
};

::lsp::json::Object wait_for_response_object(test::MemoryStream &stream) {
  EXPECT_TRUE(test::wait_until(
      [&]() {
        try {
          const auto parsed = test::parse_last_written_message(stream.written());
          return !parsed.isNull();
        } catch (...) {
          return false;
        }
      },
      std::chrono::seconds(3)));
  return test::parse_last_written_message(stream.written()).object();
}

class LanguageServerWorkspaceHandlersTest : public ::testing::Test {
protected:
  std::unique_ptr<services::SharedServices> shared = test::make_shared_services();
  DefaultLanguageServer server{*shared};
  LanguageServerRuntimeState runtimeState;
  LanguageServerHandlerContext context{server, *shared, runtimeState};

  struct HandlerHarness {
    test::MemoryStream stream;
    ::lsp::Connection connection;
    ::lsp::MessageHandler handler;

    HandlerHarness() : connection(stream), handler(connection) {}
  };

  void SetUp() override { runtimeState.setInitialized(true); }

  std::unique_ptr<HandlerHarness> makeHarness() {
    auto harness = std::make_unique<HandlerHarness>();
    addLanguageServerWorkspaceHandlers(context, harness->handler, *shared, {});
    return harness;
  }
};

TEST_F(LanguageServerWorkspaceHandlersTest,
       ExecuteCommandRequestUsesSharedHandler) {
  auto executeHandler = std::make_unique<TestExecuteCommandHandler>();
  auto *recording = executeHandler.get();
  shared->lsp.executeCommandHandler = std::move(executeHandler);

  auto harness = makeHarness();

  ::lsp::ExecuteCommandParams params{};
  params.command = "pegium.testCommand";
  params.arguments = ::lsp::LSPArray{::lsp::LSPAny{1}, ::lsp::LSPAny{true}};

  harness->stream.pushInput(test::make_request_message(
      1, ::lsp::requests::Workspace_ExecuteCommand::Method, params));
  harness->handler.processIncomingMessages();

  ASSERT_TRUE(test::wait_until([&]() {
    return recording->lastCommand == "pegium.testCommand" &&
           !harness->stream.written().empty();
  }, std::chrono::seconds(3)));
  EXPECT_EQ(recording->lastCommand, "pegium.testCommand");
  EXPECT_EQ(recording->lastArgumentCount, 2u);

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_EQ(response.get("id").integer(), 1);
  const auto &result = response.get("result").object();
  EXPECT_TRUE(result.get("handled").boolean());
  EXPECT_EQ(result.get("argumentCount").integer(), 2);
}

TEST_F(LanguageServerWorkspaceHandlersTest,
       ExecuteCommandRequestPropagatesNullResults) {
  auto executeHandler = std::make_unique<TestExecuteCommandHandler>(
      TestExecuteCommandHandler::Mode::ReturnsNull);
  auto *recording = executeHandler.get();
  shared->lsp.executeCommandHandler = std::move(executeHandler);

  auto harness = makeHarness();

  ::lsp::ExecuteCommandParams params{};
  params.command = "pegium.testCommand";

  harness->stream.pushInput(test::make_request_message(
      2, ::lsp::requests::Workspace_ExecuteCommand::Method, params));
  harness->handler.processIncomingMessages();

  ASSERT_TRUE(test::wait_until([&]() {
    return recording->lastCommand == "pegium.testCommand" &&
           !harness->stream.written().empty();
  }, std::chrono::seconds(3)));

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_EQ(response.get("id").integer(), 2);
  EXPECT_TRUE(response.get("result").isNull());
}

TEST_F(LanguageServerWorkspaceHandlersTest,
       WorkspaceSymbolRequestUsesSharedProvider) {
  shared->workspace.documentBuilder =
      std::make_unique<test::RecordingDocumentBuilder>();
  auto provider = std::make_unique<TestWorkspaceSymbolProvider>();
  auto *recording = provider.get();
  shared->lsp.workspaceSymbolProvider = std::move(provider);

  auto harness = makeHarness();

  ::lsp::WorkspaceSymbolParams params{};
  params.query = "val";

  harness->stream.pushInput(test::make_request_message(
      3, ::lsp::requests::Workspace_Symbol::Method, params));
  harness->handler.processIncomingMessages();

  ASSERT_TRUE(test::wait_until([&]() {
    return recording->lastQuery == "val" && !harness->stream.written().empty();
  }, std::chrono::seconds(3)));
  EXPECT_EQ(recording->lastQuery, "val");

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_EQ(response.get("id").integer(), 3);
  const auto &result = response.get("result").array();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front().object().get("name").string(), "value");
}

} // namespace
} // namespace pegium::lsp
