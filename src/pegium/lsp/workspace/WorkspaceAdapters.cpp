#include <pegium/lsp/workspace/WorkspaceAdapters.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

#include <lsp/messages.h>

#include <pegium/lsp/support/JsonValue.hpp>

namespace pegium {

namespace {

::lsp::RegistrationParams
make_did_change_configuration_registration(
    const std::vector<std::string> &sections) {
  ::lsp::DidChangeConfigurationRegistrationOptions options{};
  if (sections.size() == 1) {
    options.section = sections.front();
  } else if (!sections.empty()) {
    ::lsp::Array<::lsp::String> values;
    values.reserve(sections.size());
    for (const auto &section : sections) {
      values.push_back(section);
    }
    options.section = std::move(values);
  }

  ::lsp::Registration registration{};
  registration.id = "pegium.workspace.didChangeConfiguration";
  registration.method =
      std::string(::lsp::notifications::Workspace_DidChangeConfiguration::Method);
  registration.registerOptions = ::lsp::toJson(std::move(options));

  ::lsp::RegistrationParams params{};
  params.registrations.push_back(std::move(registration));
  return params;
}

::lsp::ConfigurationParams
make_workspace_configuration_params(const std::vector<std::string> &sections) {
  ::lsp::ConfigurationParams params{};
  params.items.reserve(sections.size());
  for (const auto &section : sections) {
    ::lsp::ConfigurationItem item{};
    item.section = section;
    params.items.push_back(std::move(item));
  }
  return params;
}

} // namespace

workspace::InitializeParams
to_workspace_initialize_params(const ::lsp::InitializeParams &params) {
  workspace::InitializeParams out{};
  out.capabilities.workspaceConfiguration =
      params.capabilities.workspace.has_value() &&
      params.capabilities.workspace->configuration.value_or(false);
  out.capabilities.declarationLinkSupport =
      params.capabilities.textDocument.has_value() &&
      params.capabilities.textDocument->declaration.has_value() &&
      params.capabilities.textDocument->declaration->linkSupport.value_or(false);
  out.capabilities.definitionLinkSupport =
      params.capabilities.textDocument.has_value() &&
      params.capabilities.textDocument->definition.has_value() &&
      params.capabilities.textDocument->definition->linkSupport.value_or(false);
  out.capabilities.typeDefinitionLinkSupport =
      params.capabilities.textDocument.has_value() &&
      params.capabilities.textDocument->typeDefinition.has_value() &&
      params.capabilities.textDocument->typeDefinition->linkSupport
          .value_or(false);
  out.capabilities.implementationLinkSupport =
      params.capabilities.textDocument.has_value() &&
      params.capabilities.textDocument->implementation.has_value() &&
      params.capabilities.textDocument->implementation->linkSupport
          .value_or(false);

  if (params.workspaceFolders.has_value() && !params.workspaceFolders->isNull()) {
    for (const auto &folder : params.workspaceFolders->value()) {
      out.workspaceFolders.push_back(
          {.uri = folder.uri.toString(), .name = std::string(folder.name)});
    }
  }

  return out;
}

workspace::InitializedParams
make_workspace_initialized_params(LanguageClient *languageClient) {
  workspace::InitializedParams params{};
  if (languageClient == nullptr) {
    return params;
  }

  params.registerDidChangeConfiguration =
      [languageClient](const std::vector<std::string> &sections) {
        if (sections.empty()) {
          std::promise<void> promise;
          promise.set_value();
          return promise.get_future();
        }
        auto registration = make_did_change_configuration_registration(sections);
        return languageClient->registerCapability(std::move(registration));
      };

  params.fetchConfiguration =
      [languageClient](const std::vector<std::string> &sections) {
        if (sections.empty()) {
          std::promise<std::vector<pegium::JsonValue>> promise;
          promise.set_value({});
          return promise.get_future();
        }
        auto request = make_workspace_configuration_params(sections);
        return languageClient->fetchConfiguration(std::move(request));
      };

  return params;
}

workspace::ConfigurationChangeParams
to_configuration_change_params(const ::lsp::DidChangeConfigurationParams &params) {
  return {.settings = from_lsp_any(params.settings)};
}

} // namespace pegium
