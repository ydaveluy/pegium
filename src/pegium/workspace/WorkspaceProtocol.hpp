#pragma once

#include <functional>
#include <future>
#include <string>
#include <vector>

#include <pegium/services/JsonValue.hpp>

namespace pegium::workspace {

struct WorkspaceFolder {
  std::string uri;
  std::string name;
};

struct InitializeCapabilities {
  bool workspaceConfiguration = false;
  bool declarationLinkSupport = false;
  bool definitionLinkSupport = false;
  bool typeDefinitionLinkSupport = false;
  bool implementationLinkSupport = false;
};

struct InitializeParams {
  std::vector<WorkspaceFolder> workspaceFolders;
  InitializeCapabilities capabilities;
};

struct InitializedParams {
  std::function<std::future<void>(std::vector<std::string>)>
      registerDidChangeConfiguration;
  std::function<std::future<std::vector<services::JsonValue>>(
      std::vector<std::string>)>
      fetchConfiguration;
};

struct ConfigurationChangeParams {
  services::JsonValue settings;
};

} // namespace pegium::workspace
