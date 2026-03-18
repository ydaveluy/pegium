#include <pegium/lsp/WorkspaceAdapters.hpp>

#include <memory>
#include <stdexcept>
#include <utility>

#include <lsp/messages.h>

#include <pegium/lsp/JsonValue.hpp>

namespace pegium::lsp {

namespace {

template <typename T> std::future<T> make_ready_future(T value) {
  std::promise<T> promise;
  promise.set_value(std::move(value));
  return promise.get_future();
}

std::future<void> make_ready_future() {
  std::promise<void> promise;
  promise.set_value();
  return promise.get_future();
}

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
make_workspace_initialized_params(::lsp::MessageHandler *messageHandler) {
  workspace::InitializedParams params{};
  if (messageHandler == nullptr) {
    return params;
  }

  params.registerDidChangeConfiguration =
      [messageHandler](const std::vector<std::string> &sections) {
        if (sections.empty()) {
          return make_ready_future();
        }

        auto promise = std::make_shared<std::promise<void>>();
        auto future = promise->get_future();
        auto registration = make_did_change_configuration_registration(sections);
        (void)messageHandler
            ->sendRequest<::lsp::requests::Client_RegisterCapability>(
                std::move(registration),
                [promise](::lsp::Client_RegisterCapabilityResult &&) mutable {
                  promise->set_value();
                },
                [promise](const ::lsp::ResponseError &) mutable {
                  promise->set_value();
                });
        return future;
      };

  params.fetchConfiguration =
      [messageHandler](const std::vector<std::string> &sections) {
        if (sections.empty()) {
          return make_ready_future(std::vector<services::JsonValue>{});
        }

        auto promise =
            std::make_shared<std::promise<std::vector<services::JsonValue>>>();
        auto future = promise->get_future();
        auto request = make_workspace_configuration_params(sections);
        (void)messageHandler->sendRequest<::lsp::requests::Workspace_Configuration>(
            std::move(request),
            [promise](::lsp::Workspace_ConfigurationResult &&result) mutable {
              std::vector<services::JsonValue> values;
              values.reserve(result.size());
              for (const auto &value : result) {
                values.push_back(from_lsp_any(value));
              }
              promise->set_value(std::move(values));
            },
            [promise](const ::lsp::ResponseError &) mutable {
              promise->set_value({});
            });
        return future;
      };

  return params;
}

workspace::ConfigurationChangeParams
to_configuration_change_params(const ::lsp::DidChangeConfigurationParams &params) {
  return {.settings = from_lsp_any(params.settings)};
}

} // namespace pegium::lsp
