#include <gtest/gtest.h>

#include <chrono>
#include <vector>

#include <lsp/connection.h>
#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/WorkspaceAdapters.hpp>

namespace pegium::lsp {
namespace {

TEST(WorkspaceAdaptersTest, ConvertsInitializeParamsToWorkspaceProtocolParams) {
  ::lsp::InitializeParams params{};
  params.capabilities.workspace.emplace();
  params.capabilities.workspace->configuration = true;
  params.capabilities.textDocument.emplace();
  params.capabilities.textDocument->definition.emplace();
  params.capabilities.textDocument->definition->linkSupport = true;
  params.capabilities.textDocument->declaration.emplace();
  params.capabilities.textDocument->declaration->linkSupport = false;
  params.capabilities.textDocument->typeDefinition.emplace();
  params.capabilities.textDocument->typeDefinition->linkSupport = true;
  params.capabilities.textDocument->implementation.emplace();
  params.capabilities.textDocument->implementation->linkSupport = true;

  ::lsp::WorkspaceFolder folder{};
  folder.uri = ::lsp::DocumentUri(::lsp::Uri::parse(
      test::make_file_uri("workspace-folder")));
  folder.name = "workspace-folder";
  params.workspaceFolders = ::lsp::Array<::lsp::WorkspaceFolder>{folder};

  const auto result = to_workspace_initialize_params(params);
  EXPECT_TRUE(result.capabilities.workspaceConfiguration);
  EXPECT_FALSE(result.capabilities.declarationLinkSupport);
  EXPECT_TRUE(result.capabilities.definitionLinkSupport);
  EXPECT_TRUE(result.capabilities.typeDefinitionLinkSupport);
  EXPECT_TRUE(result.capabilities.implementationLinkSupport);
  ASSERT_EQ(result.workspaceFolders.size(), 1u);
  EXPECT_EQ(result.workspaceFolders.front().uri, folder.uri.toString());
  EXPECT_EQ(result.workspaceFolders.front().name, "workspace-folder");
}

TEST(WorkspaceAdaptersTest,
     MissingGotoLinkCapabilitiesDefaultToFalseInWorkspaceProtocolParams) {
  ::lsp::InitializeParams params{};
  params.capabilities.workspace.emplace();
  params.capabilities.workspace->configuration = true;
  params.capabilities.textDocument.emplace();

  const auto result = to_workspace_initialize_params(params);
  EXPECT_TRUE(result.capabilities.workspaceConfiguration);
  EXPECT_FALSE(result.capabilities.declarationLinkSupport);
  EXPECT_FALSE(result.capabilities.definitionLinkSupport);
  EXPECT_FALSE(result.capabilities.typeDefinitionLinkSupport);
  EXPECT_FALSE(result.capabilities.implementationLinkSupport);
}

TEST(WorkspaceAdaptersTest, InitializedParamsWithNullMessageHandlerStayEmpty) {
  const auto params = make_workspace_initialized_params(nullptr);
  EXPECT_FALSE(params.registerDidChangeConfiguration);
  EXPECT_FALSE(params.fetchConfiguration);
}

TEST(WorkspaceAdaptersTest,
     RegisterDidChangeConfigurationRequestsClientCapabilityRegistration) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  const auto params = make_workspace_initialized_params(&handler);
  ASSERT_TRUE(params.registerDidChangeConfiguration);

  auto future =
      params.registerDidChangeConfiguration({"pegium", "arithmetics"});

  const auto request = test::parse_last_written_message(stream.written()).object();
  EXPECT_EQ(request.get("method").string(),
            ::lsp::requests::Client_RegisterCapability::Method);

  const auto requestId = request.get("id").integer();
  const auto &registrations =
      request.get("params").object().get("registrations").array();
  ASSERT_EQ(registrations.size(), 1u);
  const auto &registration = registrations.front().object();
  EXPECT_EQ(registration.get("id").string(),
            "pegium.workspace.didChangeConfiguration");
  EXPECT_EQ(registration.get("method").string(),
            ::lsp::notifications::Workspace_DidChangeConfiguration::Method);

  const auto &options =
      registration.get("registerOptions").object().get("section").array();
  ASSERT_EQ(options.size(), 2u);
  EXPECT_EQ(options[0].string(), "pegium");
  EXPECT_EQ(options[1].string(), "arithmetics");

  stream.pushInput(test::make_response_message(
      requestId, ::lsp::Client_RegisterCapabilityResult{}));
  handler.processIncomingMessages();

  EXPECT_EQ(future.wait_for(std::chrono::milliseconds(0)),
            std::future_status::ready);
}

TEST(WorkspaceAdaptersTest,
     FetchConfigurationRequestsSectionsAndConvertsResponseValues) {
  test::MemoryStream stream;
  ::lsp::Connection connection(stream);
  ::lsp::MessageHandler handler(connection);

  const auto params = make_workspace_initialized_params(&handler);
  ASSERT_TRUE(params.fetchConfiguration);

  auto future = params.fetchConfiguration({"pegium.validation", "arithmetics"});

  const auto request = test::parse_last_written_message(stream.written()).object();
  EXPECT_EQ(request.get("method").string(),
            ::lsp::requests::Workspace_Configuration::Method);
  const auto requestId = request.get("id").integer();

  const auto &items = request.get("params").object().get("items").array();
  ASSERT_EQ(items.size(), 2u);
  EXPECT_EQ(items[0].object().get("section").string(), "pegium.validation");
  EXPECT_EQ(items[1].object().get("section").string(), "arithmetics");

  ::lsp::Workspace_ConfigurationResult response;
  response.push_back(::lsp::LSPAny{true});
  ::lsp::LSPObject settings;
  settings["severity"] = ::lsp::LSPAny{::lsp::String("warning")};
  settings["limit"] = ::lsp::LSPAny{7};
  response.push_back(::lsp::LSPAny{std::move(settings)});

  stream.pushInput(test::make_response_message(requestId, std::move(response)));
  handler.processIncomingMessages();

  auto values = future.get();
  ASSERT_EQ(values.size(), 2u);
  EXPECT_TRUE(values[0].boolean());
  ASSERT_TRUE(values[1].isObject());
  EXPECT_EQ(values[1].object().at("severity").string(), "warning");
  EXPECT_EQ(values[1].object().at("limit").integer(), 7);
}

TEST(WorkspaceAdaptersTest, ConvertsConfigurationChangeParamsToJsonValue) {
  ::lsp::DidChangeConfigurationParams params{};
  ::lsp::LSPObject settings;
  settings["strict"] = ::lsp::LSPAny{true};
  settings["threshold"] = ::lsp::LSPAny{3};
  params.settings = ::lsp::LSPAny{std::move(settings)};

  const auto result = to_configuration_change_params(params);
  ASSERT_TRUE(result.settings.isObject());
  EXPECT_TRUE(result.settings.object().at("strict").boolean());
  EXPECT_EQ(result.settings.object().at("threshold").integer(), 3);
}

} // namespace
} // namespace pegium::lsp
