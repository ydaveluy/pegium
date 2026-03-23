#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/hierarchy/CallHierarchyProvider.hpp>
#include <pegium/lsp/runtime/DefaultLanguageServer.hpp>
#include <pegium/lsp/runtime/LanguageServerHandlerContext.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlerParts.hpp>
#include <pegium/lsp/navigation/RenameProvider.hpp>
#include <pegium/lsp/navigation/TypeDefinitionProvider.hpp>

namespace pegium {
namespace {

class TestPrepareRenameProvider final : public ::pegium::RenameProvider {
public:
  enum class Mode : std::uint8_t {
    Null,
    Range,
    Placeholder,
    DefaultBehavior,
  };

  explicit TestPrepareRenameProvider(Mode mode) : mode(mode) {}

  void setMode(Mode value) { mode = value; }

  std::optional<::lsp::PrepareRenameResult>
  prepareRename(const workspace::Document &, const ::lsp::TextDocumentPositionParams &,
                const utils::CancellationToken &) const override {
    switch (mode) {
    case Mode::Null:
      return std::nullopt;
    case Mode::Range: {
      ::lsp::Range range{};
      range.start = text::Position(2, 1);
      range.end = text::Position(2, 6);
      ::lsp::PrepareRenameResult result{};
      result = std::move(range);
      return result;
    }
    case Mode::Placeholder: {
      ::lsp::PrepareRenameResult_Range_Placeholder value{};
      value.range.start = text::Position(3, 0);
      value.range.end = text::Position(3, 4);
      value.placeholder = "name";
      ::lsp::PrepareRenameResult result{};
      result = std::move(value);
      return result;
    }
    case Mode::DefaultBehavior: {
      ::lsp::PrepareRenameResult_DefaultBehavior value{};
      value.defaultBehavior = true;
      ::lsp::PrepareRenameResult result{};
      result = std::move(value);
      return result;
    }
    }
    return std::nullopt;
  }

private:
  Mode mode;
};

class TestTypeDefinitionProvider final : public ::pegium::TypeDefinitionProvider {
public:
  std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &, const ::lsp::TypeDefinitionParams &,
                    const utils::CancellationToken &) const override {
    ::lsp::LocationLink link{};
    link.targetUri = ::lsp::DocumentUri(
        ::lsp::Uri::parse(test::make_file_uri("type-definition-target.test")));
    link.targetRange.start = text::Position(4, 0);
    link.targetRange.end = text::Position(4, 10);
    link.targetSelectionRange.start = text::Position(4, 3);
    link.targetSelectionRange.end = text::Position(4, 8);
    link.originSelectionRange = ::lsp::Range{
        .start = text::Position(1, 1), .end = text::Position(1, 5)};
    return std::vector<::lsp::LocationLink>{std::move(link)};
  }
};

class TestCallHierarchyProvider final : public ::pegium::CallHierarchyProvider {
public:
  std::vector<::lsp::CallHierarchyItem>
  prepareCallHierarchy(const workspace::Document &,
                       const ::lsp::CallHierarchyPrepareParams &,
                       const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::CallHierarchyIncomingCall>
  incomingCalls(const ::lsp::CallHierarchyIncomingCallsParams &,
                const utils::CancellationToken &) const override {
    return {};
  }

  std::vector<::lsp::CallHierarchyOutgoingCall>
  outgoingCalls(const ::lsp::CallHierarchyOutgoingCallsParams &,
                const utils::CancellationToken &) const override {
    return {};
  }
};

std::shared_ptr<workspace::Document>
register_document(pegium::SharedServices &sharedServices,
                  std::string_view fileName = "request-shape.test") {
  auto document = test::open_and_build_document(
      sharedServices, test::make_file_uri(fileName), "test", "alpha");
  EXPECT_NE(document, nullptr);
  return document;
}

LanguageServerHandlerContext make_context(DefaultLanguageServer &server,
                                          pegium::SharedServices &shared,
                                          LanguageServerRuntimeState &runtimeState,
                                          workspace::InitializeCapabilities caps = {}) {
  runtimeState.setInitialized(true);
  LanguageServerHandlerContext context(server, shared, runtimeState);
  context.setInitializeCapabilities(std::move(caps));
  return context;
}

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

void expect_missing_service_request_failed(const ::lsp::json::Object &response,
                                           std::string_view uri) {
  ASSERT_TRUE(response.contains("error"));
  const auto &error = response.get("error").object();
  EXPECT_EQ(error.get("code").integer(),
            static_cast<int>(::lsp::MessageError::RequestFailed));
  EXPECT_EQ(error.get("message").string(),
            "Could not find service instance for uri: '" + std::string(uri) +
                "'");
}

class LanguageServerTextDocumentHandlersTest : public ::testing::Test {
protected:
  std::unique_ptr<pegium::SharedServices> shared = test::make_empty_shared_services();
  DefaultLanguageServer server{*shared};
  LanguageServerRuntimeState runtimeState;

  LanguageServerTextDocumentHandlersTest() {
    pegium::services::installDefaultSharedCoreServices(*shared);
    pegium::installDefaultSharedLspServices(*shared);
    pegium::test::initialize_shared_workspace_for_tests(*shared);
  }

  struct HandlerHarness {
    test::MemoryStream stream;
    ::lsp::Connection connection;
    ::lsp::MessageHandler handler;

    HandlerHarness() : connection(stream), handler(connection) {}
  };

  template <typename Configure>
  std::shared_ptr<workspace::Document>
  registerTestLanguageAndDocument(Configure &&configure,
                                  std::string_view fileName) {
    auto services = test::make_uninstalled_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*services);
    pegium::installDefaultLspServices(*services);
    configure(*services);
    shared->serviceRegistry->registerServices(std::move(services));
    if (testing::Test::HasFatalFailure() || testing::Test::HasNonfatalFailure()) {
      return nullptr;
    }
    auto document = register_document(*shared, fileName);
    EXPECT_NE(document, nullptr);
    return document;
  }

  LanguageServerHandlerContext makeContext(
      workspace::InitializeCapabilities capabilities = {}) {
    return make_context(server, *shared, runtimeState, std::move(capabilities));
  }

  std::unique_ptr<HandlerHarness>
  makeHarness(LanguageServerHandlerContext &context) {
    auto harness = std::make_unique<HandlerHarness>();
    addLanguageServerTextDocumentHandlers(context, harness->handler, *shared, {});
    return harness;
  }
};

TEST_F(LanguageServerTextDocumentHandlersTest,
       PrepareRenameNullResponseStaysNull) {
  auto document = registerTestLanguageAndDocument(
      [](auto &services) {
        services.lsp.renameProvider = std::make_unique<TestPrepareRenameProvider>(
            TestPrepareRenameProvider::Mode::Null);
      },
      "prepare-rename-null.test");
  ASSERT_NE(document, nullptr);

  auto context = makeContext();
  auto harness = makeHarness(context);

  ::lsp::PrepareRenameParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.position = text::Position(0, 0);

  harness->stream.pushInput(test::make_request_message(
      1, ::lsp::requests::TextDocument_PrepareRename::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_TRUE(response.get("result").isNull());
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       PrepareRenameSupportsRangePlaceholderAndDefaultBehaviorShapes) {
  TestPrepareRenameProvider *recording = nullptr;
  auto document = registerTestLanguageAndDocument(
      [&recording](auto &services) {
        auto provider = std::make_unique<TestPrepareRenameProvider>(
            TestPrepareRenameProvider::Mode::Range);
        recording = provider.get();
        services.lsp.renameProvider = std::move(provider);
      },
      "prepare-rename-shapes.test");
  ASSERT_NE(document, nullptr);
  ASSERT_NE(recording, nullptr);

  auto context = makeContext();
  auto harness = makeHarness(context);

  auto send_request = [&](int id) {
    harness->stream.clearWritten();
    ::lsp::PrepareRenameParams params{};
    params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
    params.position = text::Position(0, 0);
    harness->stream.pushInput(test::make_request_message(
        id, ::lsp::requests::TextDocument_PrepareRename::Method, params));
    harness->handler.processIncomingMessages();
    return wait_for_response_object(harness->stream);
  };

  auto response = send_request(1);
  ASSERT_TRUE(response.get("result").isObject());
  EXPECT_EQ(
      response.get("result").object().get("start").object().get("line").integer(),
      2);

  recording->setMode(TestPrepareRenameProvider::Mode::Placeholder);
  response = send_request(2);
  ASSERT_TRUE(response.get("result").isObject());
  EXPECT_EQ(response.get("result").object().get("placeholder").string(), "name");
  EXPECT_EQ(response.get("result")
                .object()
                .get("range")
                .object()
                .get("start")
                .object()
                .get("line")
                .integer(),
            3);

  recording->setMode(TestPrepareRenameProvider::Mode::DefaultBehavior);
  response = send_request(3);
  ASSERT_TRUE(response.get("result").isObject());
  EXPECT_TRUE(response.get("result").object().get("defaultBehavior").boolean());
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       PrepareCallHierarchyReturnsNullForEmptyCollections) {
  auto document = registerTestLanguageAndDocument(
      [](auto &services) {
        services.lsp.callHierarchyProvider =
            std::make_unique<TestCallHierarchyProvider>();
      },
      "prepare-call-hierarchy.test");
  ASSERT_NE(document, nullptr);

  auto context = makeContext();
  auto harness = makeHarness(context);

  ::lsp::CallHierarchyPrepareParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.position = text::Position(0, 0);

  harness->stream.pushInput(test::make_request_message(
      4, ::lsp::requests::TextDocument_PrepareCallHierarchy::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_TRUE(response.get("result").isNull());
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       PrepareRenameReturnsNullWhenNoServiceMatchesUri) {
  auto context = makeContext();
  auto harness = makeHarness(context);

  ::lsp::PrepareRenameParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(
      ::lsp::Uri::parse(test::make_file_uri("unknown-service.test")));
  params.position = text::Position(0, 0);

  harness->stream.pushInput(test::make_request_message(
      7, ::lsp::requests::TextDocument_PrepareRename::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  EXPECT_TRUE(response.get("result").isNull());
  EXPECT_FALSE(response.contains("error"));
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       IncomingCallHierarchyReturnsRequestFailedWhenNoServiceMatchesItemUri) {
  auto context = makeContext();
  auto harness = makeHarness(context);

  const auto uri = test::make_file_uri("unknown-call-hierarchy.test");
  ::lsp::CallHierarchyIncomingCallsParams params{};
  params.item.name = "Missing";
  params.item.kind = ::lsp::SymbolKind::Function;
  params.item.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  params.item.range.start = text::Position(0, 0);
  params.item.range.end = text::Position(0, 7);
  params.item.selectionRange = params.item.range;

  harness->stream.pushInput(test::make_request_message(
      8, ::lsp::requests::CallHierarchy_IncomingCalls::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  expect_missing_service_request_failed(response, uri);
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       OutgoingCallHierarchyReturnsRequestFailedWhenNoServiceMatchesItemUri) {
  auto context = makeContext();
  auto harness = makeHarness(context);

  const auto uri = test::make_file_uri("unknown-call-hierarchy.test");
  ::lsp::CallHierarchyOutgoingCallsParams params{};
  params.item.name = "Missing";
  params.item.kind = ::lsp::SymbolKind::Function;
  params.item.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  params.item.range.start = text::Position(0, 0);
  params.item.range.end = text::Position(0, 7);
  params.item.selectionRange = params.item.range;

  harness->stream.pushInput(test::make_request_message(
      9, ::lsp::requests::CallHierarchy_OutgoingCalls::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  expect_missing_service_request_failed(response, uri);
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       TypeHierarchySupertypesReturnsRequestFailedWhenNoServiceMatchesItemUri) {
  auto context = makeContext();
  auto harness = makeHarness(context);

  const auto uri = test::make_file_uri("unknown-type-hierarchy.test");
  ::lsp::TypeHierarchySupertypesParams params{};
  params.item.name = "Missing";
  params.item.kind = ::lsp::SymbolKind::Class;
  params.item.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  params.item.range.start = text::Position(0, 0);
  params.item.range.end = text::Position(0, 5);
  params.item.selectionRange = params.item.range;

  harness->stream.pushInput(test::make_request_message(
      10, ::lsp::requests::TypeHierarchy_Supertypes::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  expect_missing_service_request_failed(response, uri);
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       TypeHierarchySubtypesReturnsRequestFailedWhenNoServiceMatchesItemUri) {
  auto context = makeContext();
  auto harness = makeHarness(context);

  const auto uri = test::make_file_uri("unknown-type-hierarchy.test");
  ::lsp::TypeHierarchySubtypesParams params{};
  params.item.name = "Missing";
  params.item.kind = ::lsp::SymbolKind::Class;
  params.item.uri = ::lsp::DocumentUri(::lsp::Uri::parse(uri));
  params.item.range.start = text::Position(0, 0);
  params.item.range.end = text::Position(0, 5);
  params.item.selectionRange = params.item.range;

  harness->stream.pushInput(test::make_request_message(
      11, ::lsp::requests::TypeHierarchy_Subtypes::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  expect_missing_service_request_failed(response, uri);
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       TypeDefinitionReturnsLocationLinksWhenClientSupportsThem) {
  auto document = registerTestLanguageAndDocument(
      [](auto &services) {
        services.lsp.typeProvider = std::make_unique<TestTypeDefinitionProvider>();
      },
      "type-definition-links.test");
  ASSERT_NE(document, nullptr);

  workspace::InitializeCapabilities capabilities;
  capabilities.typeDefinitionLinkSupport = true;
  auto context = makeContext(capabilities);
  auto harness = makeHarness(context);

  ::lsp::TypeDefinitionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.position = text::Position(0, 0);

  harness->stream.pushInput(test::make_request_message(
      5, ::lsp::requests::TextDocument_TypeDefinition::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  ASSERT_TRUE(response.get("result").isArray());
  const auto &result = response.get("result").array();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_EQ(result.front()
                .object()
                .get("targetSelectionRange")
                .object()
                .get("start")
                .object()
                .get("character")
                .integer(),
            3);
  EXPECT_TRUE(result.front().object().contains("originSelectionRange"));
}

TEST_F(LanguageServerTextDocumentHandlersTest,
       TypeDefinitionFallsBackToLocationsWhenClientDoesNotSupportLinks) {
  auto document = registerTestLanguageAndDocument(
      [](auto &services) {
        services.lsp.typeProvider = std::make_unique<TestTypeDefinitionProvider>();
      },
      "type-definition-locations.test");
  ASSERT_NE(document, nullptr);

  auto context = makeContext();
  auto harness = makeHarness(context);

  ::lsp::TypeDefinitionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.position = text::Position(0, 0);

  harness->stream.pushInput(test::make_request_message(
      6, ::lsp::requests::TextDocument_TypeDefinition::Method, params));
  harness->handler.processIncomingMessages();

  const auto response = wait_for_response_object(harness->stream);
  ASSERT_TRUE(response.get("result").isArray());
  const auto &result = response.get("result").array();
  ASSERT_EQ(result.size(), 1u);
  EXPECT_TRUE(result.front().object().contains("uri"));
  EXPECT_FALSE(result.front().object().contains("targetUri"));
  EXPECT_EQ(result.front()
                .object()
                .get("range")
                .object()
                .get("start")
                .object()
                .get("character")
                .integer(),
            3);
}

} // namespace
} // namespace pegium
