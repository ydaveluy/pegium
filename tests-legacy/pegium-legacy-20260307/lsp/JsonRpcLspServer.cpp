#include <gtest/gtest.h>

#include <future>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <lsp/connection.h>
#include <lsp/io/stream.h>

#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/NodeKindProvider.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/syntax-tree/AstNode.hpp>

namespace {

std::string lsp_message(std::string_view json) {
  return "Content-Length: " + std::to_string(json.size()) + "\r\n" +
         "Content-Type: application/vscode-jsonrpc; charset=utf-8\r\n\r\n" +
         std::string(json);
}

struct ScriptedStream final : lsp::io::Stream {
  explicit ScriptedStream(std::string inputData)
      : input(std::move(inputData)) {}

  std::string input;
  std::size_t readOffset = 0;
  std::string written;

  void read(char *buffer, std::size_t size) override {
    for (std::size_t index = 0; index < size; ++index) {
      if (readOffset < input.size()) {
        buffer[index] = input[readOffset++];
      } else {
        buffer[index] = Eof;
      }
    }
  }

  void write(const char *buffer, std::size_t size) override {
    written.append(buffer, size);
  }
};

struct FailingWriteStream final : lsp::io::Stream {
  explicit FailingWriteStream(std::string inputData)
      : input(std::move(inputData)) {}

  std::string input;
  std::size_t readOffset = 0;

  void read(char *buffer, std::size_t size) override {
    for (std::size_t index = 0; index < size; ++index) {
      if (readOffset < input.size()) {
        buffer[index] = input[readOffset++];
      } else {
        buffer[index] = Eof;
      }
    }
  }

  void write(const char *, std::size_t) override {
    throw lsp::io::Error("Broken pipe");
  }
};


struct CountingNodeKindProvider final : pegium::lsp::NodeKindProvider {
  mutable std::size_t calls = 0;
  mutable std::string lastKind;

  ::lsp::SymbolKindEnum toSymbolKind(std::string_view kind) const override {
    ++calls;
    lastKind = std::string(kind);
    return ::lsp::SymbolKind::Class;
  }
};

struct FixedWorkspaceSymbolProvider final
    : pegium::services::WorkspaceSymbolProvider {
  std::vector<pegium::services::WorkspaceSymbol>
  getWorkspaceSymbols(const ::lsp::WorkspaceSymbolParams &,
                      const pegium::utils::CancellationToken &cancelToken) const override {
    (void)cancelToken;
    return std::vector<pegium::services::WorkspaceSymbol>{{
        .name = "MySymbol",
        .kind = "custom-kind",
        .uri = "file:///demo.pg",
        .begin = 0,
        .end = 8,
        .containerName = {},
    }};
  }
};

struct InjectedWorkspaceSymbolServer final : pegium::lsp::DefaultLanguageServer {
  using pegium::lsp::DefaultLanguageServer::DefaultLanguageServer;

  std::vector<pegium::services::WorkspaceSymbol>
  getWorkspaceSymbols(
      const ::lsp::WorkspaceSymbolParams &,
      const pegium::utils::CancellationToken &) const override {
    return std::vector<pegium::services::WorkspaceSymbol>{{
        .name = "InjectedSymbol",
        .kind = "custom-kind",
        .uri = "file:///injected.pg",
        .begin = 0,
        .end = 14,
        .containerName = {},
    }};
  }
};

struct KeywordParser final : pegium::parser::Parser {
  void parse(pegium::workspace::Document &document,
             const pegium::utils::CancellationToken & = {}) const override {
    pegium::parser::ParseResult result;
    result.parsedLength =
        static_cast<pegium::TextOffset>(document.text().size());
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

struct RecordingDocumentUpdateHandler final
    : pegium::lsp::DocumentUpdateHandler {
  std::size_t didOpenCalls = 0;
  std::string lastUri;
  std::string lastLanguageId;
  std::string lastText;
  std::optional<std::int64_t> lastVersion;

  std::future<void> didOpen(std::string uri, std::string languageId,
                            std::string text,
                            std::optional<std::int64_t> clientVersion) override {
    ++didOpenCalls;
    lastUri = std::move(uri);
    lastLanguageId = std::move(languageId);
    lastText = std::move(text);
    lastVersion = clientVersion;
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::future<void> didChange(
      std::string_view, std::int64_t,
      const std::vector<::lsp::TextDocumentContentChangeEvent> &) override {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::future<void> didSave(
      std::string_view, std::optional<std::string>) override {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::future<void> didClose(std::string_view) override {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::future<void> didChangeWatchedFiles(
      const ::lsp::DidChangeWatchedFilesParams &) override {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  pegium::utils::ScopedDisposable onWatchedFilesChange(
      std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>)
      override {
    return {};
  }
};

} // namespace

TEST(ArchitectureTest, DefaultLanguageServerHandlesInitializeShutdownExit) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"exit"})");

  ScriptedStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  shared.lsp.connection = &connection;

  ASSERT_NE(shared.lsp.languageServer, nullptr);
  EXPECT_EQ(shared.lsp.languageServer->run(), 0);
  EXPECT_NE(stream.written.find("\"id\":1"), std::string::npos);
  EXPECT_NE(stream.written.find("\"textDocumentSync\""), std::string::npos)
      << stream.written;
  EXPECT_NE(stream.written.find("\"positionEncoding\":\"utf-16\""),
            std::string::npos)
      << stream.written;
  EXPECT_NE(stream.written.find("\"change\":2"), std::string::npos);
  EXPECT_NE(stream.written.find("\"save\":true"), std::string::npos);
  EXPECT_EQ(stream.written.find("\"willSave\":true"), std::string::npos);
  EXPECT_EQ(stream.written.find("\"willSaveWaitUntil\":true"),
            std::string::npos);
  EXPECT_NE(stream.written.find("\"workspaceFolders\""), std::string::npos);
  EXPECT_NE(stream.written.find("\"fileOperations\""), std::string::npos);
  EXPECT_EQ(stream.written.find("\"willCreate\""), std::string::npos);
  EXPECT_EQ(stream.written.find("\"willRename\""), std::string::npos);
  EXPECT_EQ(stream.written.find("\"willDelete\""), std::string::npos);
}

TEST(ArchitectureTest, DefaultLanguageServerUsesInjectedNodeKindProviderForWorkspaceSymbols) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":2,"method":"workspace/symbol","params":{"query":"My"}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":3,"method":"shutdown"})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"exit"})");

  ScriptedStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  auto nodeKindProvider = std::make_unique<CountingNodeKindProvider>();
  auto *nodeKindProviderPtr = nodeKindProvider.get();
  shared.lsp.nodeKindProvider = std::move(nodeKindProvider);
  shared.lsp.workspaceSymbolProvider = std::make_unique<FixedWorkspaceSymbolProvider>();
  shared.lsp.connection = &connection;

  ASSERT_NE(shared.lsp.languageServer, nullptr);
  EXPECT_EQ(shared.lsp.languageServer->run(), 0);

  EXPECT_EQ(nodeKindProviderPtr->calls, 1u);
  EXPECT_EQ(nodeKindProviderPtr->lastKind, "custom-kind");
}

TEST(ArchitectureTest, DefaultLanguageServerUsesInjectedLanguageServerInstance) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":2,"method":"workspace/symbol","params":{"query":"Injected"}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":3,"method":"shutdown"})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"exit"})");

  ScriptedStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  auto nodeKindProvider = std::make_unique<CountingNodeKindProvider>();
  auto *nodeKindProviderPtr = nodeKindProvider.get();
  shared.lsp.nodeKindProvider = std::move(nodeKindProvider);
  shared.workspace.documents->openOrUpdate("file:///injected.pg",
                                           "InjectedSymbol", "injected");
  shared.lsp.languageServer = std::make_unique<InjectedWorkspaceSymbolServer>(shared);
  shared.lsp.connection = &connection;

  ASSERT_NE(shared.lsp.languageServer, nullptr);
  EXPECT_EQ(shared.lsp.languageServer->run(), 0);

  EXPECT_NE(stream.written.find("InjectedSymbol"), std::string::npos);
  EXPECT_EQ(nodeKindProviderPtr->calls, 1u);
  EXPECT_EQ(nodeKindProviderPtr->lastKind, "custom-kind");
}

TEST(ArchitectureTest, DefaultLanguageServerUsesInjectedDocumentUpdateHandler) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///custom.pg","languageId":"mini","version":7,"text":"hello"}}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"exit"})");

  ScriptedStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  auto documentUpdateHandler = std::make_unique<RecordingDocumentUpdateHandler>();
  auto *documentUpdateHandlerPtr = documentUpdateHandler.get();
  shared.lsp.documentUpdateHandler = std::move(documentUpdateHandler);
  shared.lsp.connection = &connection;

  ASSERT_NE(shared.lsp.languageServer, nullptr);
  EXPECT_EQ(shared.lsp.languageServer->run(), 0);
  EXPECT_EQ(documentUpdateHandlerPtr->didOpenCalls, 1u);
  EXPECT_EQ(documentUpdateHandlerPtr->lastUri, "file:///custom.pg");
  EXPECT_EQ(documentUpdateHandlerPtr->lastLanguageId, "mini");
  EXPECT_EQ(documentUpdateHandlerPtr->lastText, "hello");
  EXPECT_EQ(documentUpdateHandlerPtr->lastVersion, 7);
}

TEST(ArchitectureTest, DefaultLanguageServerPublishesDiagnosticsFromBuilderHooks) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///diag.pg","languageId":"mini","version":1,"text":"ok"}}})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///diag.pg","version":2},"contentChanges":[{"text":"ko"}]}})") +
      lsp_message(R"({"jsonrpc":"2.0","id":2,"method":"shutdown"})") +
      lsp_message(R"({"jsonrpc":"2.0","method":"exit"})");

  ScriptedStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  auto language =
      pegium::services::makeDefaultServices(
          shared, "mini", std::make_unique<KeywordParser>());
  ASSERT_TRUE(shared.serviceRegistry->registerLanguage(std::move(language)));
  shared.lsp.connection = &connection;

  ASSERT_NE(shared.lsp.languageServer, nullptr);
  EXPECT_EQ(shared.lsp.languageServer->run(), 0);
  EXPECT_NE(stream.written.find("textDocument/publishDiagnostics"),
            std::string::npos);
  EXPECT_NE(stream.written.find("\"version\":2"), std::string::npos);
}

TEST(ArchitectureTest, DefaultLanguageServerHandlesBrokenPipeWithoutThrowing) {
  const auto payload =
      lsp_message(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{"processId":null,"rootUri":null,"capabilities":{}}})");

  FailingWriteStream stream(payload);
  lsp::Connection connection(stream);

  pegium::services::SharedServices shared;
  shared.lsp.connection = &connection;

  EXPECT_NO_THROW({
    ASSERT_NE(shared.lsp.languageServer, nullptr);
    const auto exitCode = shared.lsp.languageServer->run();
    EXPECT_EQ(exitCode, 1);
  });
}
