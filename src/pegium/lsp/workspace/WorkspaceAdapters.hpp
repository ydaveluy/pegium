#pragma once

#include <future>

#include <lsp/types.h>

#include <pegium/lsp/runtime/LanguageClient.hpp>
#include <pegium/core/workspace/WorkspaceProtocol.hpp>

namespace pegium {

/// Adapts LSP initialize params to the workspace runtime model.
[[nodiscard]] workspace::InitializeParams
to_workspace_initialize_params(const ::lsp::InitializeParams &params);

/// Creates the workspace initialized event payload.
[[nodiscard]] workspace::InitializedParams
make_workspace_initialized_params(LanguageClient *languageClient);

/// Adapts configuration changes to the workspace runtime model.
[[nodiscard]] workspace::ConfigurationChangeParams
to_configuration_change_params(const ::lsp::DidChangeConfigurationParams &params);

} // namespace pegium
