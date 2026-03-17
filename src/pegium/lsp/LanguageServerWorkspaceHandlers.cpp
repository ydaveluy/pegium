#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>

#include <utility>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/LanguageServerFeatures.hpp>
#include <pegium/lsp/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

void addLanguageServerWorkspaceHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    services::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements) {
  const auto workspaceSymbolRequirement = requirement_or(
      serviceRequirements.WorkspaceSymbolProvider,
      WorkspaceState::IndexedContent);

  if (sharedServices.lsp.executeCommandHandler != nullptr) {
    handler.add<::lsp::requests::Workspace_ExecuteCommand>(
        make_async_request<::lsp::Workspace_ExecuteCommandResult>(
            server,
            [&server, &sharedServices](::lsp::ExecuteCommandParams &&params,
                                       const utils::CancellationToken &cancelToken)
                -> std::future<::lsp::Workspace_ExecuteCommandResult> {
              ensure_initialized(server);
              static const ::lsp::LSPArray emptyArguments{};
              const auto &arguments = params.arguments.has_value()
                                          ? *params.arguments
                                          : emptyArguments;
              return adapt_async_result<::lsp::Workspace_ExecuteCommandResult>(
                  server,
                  executeCommand(sharedServices, params.command, arguments,
                                 cancelToken),
                  wrap_optional_payload<::lsp::Workspace_ExecuteCommandResult>{},
                  cancelToken);
            }));
  }

  handler.add<::lsp::requests::Workspace_Symbol>(
      create_server_request_handler<::lsp::Workspace_SymbolResult,
                                    ::lsp::WorkspaceSymbolParams>(
          server, sharedServices, workspaceSymbolRequirement,
          [&sharedServices](const ::lsp::WorkspaceSymbolParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getWorkspaceSymbols(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::Workspace_SymbolResult>{}));

  if (sharedServices.lsp.workspaceSymbolProvider != nullptr &&
      sharedServices.lsp.workspaceSymbolProvider->supportsResolveSymbol()) {
    handler.add<::lsp::requests::WorkspaceSymbol_Resolve>(
        make_async_request<::lsp::WorkspaceSymbol>(
            server,
            [&server, &sharedServices,
             workspaceSymbolRequirement](::lsp::WorkspaceSymbol &&symbol,
                                         const utils::CancellationToken &cancelToken)
                -> std::future<::lsp::WorkspaceSymbol> {
              ensure_initialized(server);
              wait_until_phase(sharedServices, cancelToken, std::nullopt,
                               workspaceSymbolRequirement);
              return adapt_async_result<::lsp::WorkspaceSymbol>(
                  server,
                  resolveWorkspaceSymbol(sharedServices, symbol, cancelToken),
                  wrap_resolved_or_original<::lsp::WorkspaceSymbol>{
                      std::move(symbol)},
                  cancelToken);
            }));
  }
}

} // namespace pegium::lsp
