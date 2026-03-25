#pragma once

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <pegium/core/services/JsonValue.hpp>

namespace pegium::workspace {

/// Workspace folder descriptor received from the client.
struct WorkspaceFolder {
  std::string uri;
  std::string name;
};

/// Subset of client capabilities relevant to core workspace services.
struct InitializeCapabilities {
  bool workspaceConfiguration = false;
  bool declarationLinkSupport = false;
  bool definitionLinkSupport = false;
  bool typeDefinitionLinkSupport = false;
  bool implementationLinkSupport = false;
};

/// Parameters received during the initialize request.
struct InitializeParams {
  std::vector<WorkspaceFolder> workspaceFolders;
  InitializeCapabilities capabilities;
};

/// Callbacks received after the initialize handshake completed.
struct InitializedParams {
  std::function<std::future<void>(std::vector<std::string>)>
      registerDidChangeConfiguration;
  std::function<std::future<std::vector<pegium::JsonValue>>(
      std::vector<std::string>)>
      fetchConfiguration;
};

/// Configuration change notification payload.
struct ConfigurationChangeParams {
  pegium::JsonValue settings;
};

} // namespace pegium::workspace
