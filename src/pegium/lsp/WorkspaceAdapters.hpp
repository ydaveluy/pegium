#pragma once

#include <future>

#include <lsp/messagehandler.h>
#include <lsp/types.h>

#include <pegium/workspace/WorkspaceProtocol.hpp>

namespace pegium::lsp {

[[nodiscard]] workspace::InitializeParams
to_workspace_initialize_params(const ::lsp::InitializeParams &params);

[[nodiscard]] workspace::InitializedParams
make_workspace_initialized_params(::lsp::MessageHandler *messageHandler);

[[nodiscard]] workspace::ConfigurationChangeParams
to_configuration_change_params(const ::lsp::DidChangeConfigurationParams &params);

} // namespace pegium::lsp
