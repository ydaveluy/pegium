#include <pegium/lsp/runtime/LanguageServerRequestHandlerParts.hpp>

#include <utility>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/services/LanguageServerFeatures.hpp>
#include <pegium/lsp/runtime/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

void addLanguageServerWorkspaceHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    pegium::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements) {
  const auto workspaceSymbolRequirement =
      serviceRequirements.WorkspaceSymbolProvider.value_or(
      WorkspaceState::IndexedContent);

  if (sharedServices.lsp.executeCommandHandler != nullptr) {
    add_request_handler<::lsp::requests::Workspace_ExecuteCommand>(
        handler,
        make_async_request<::lsp::requests::Workspace_ExecuteCommand>(
            server,
            [&server, &sharedServices](const ::lsp::ExecuteCommandParams &params,
                                       const utils::CancellationToken &cancelToken) {
              ensure_initialized(server);
              static const ::lsp::LSPArray emptyArguments{};
              const auto &arguments = params.arguments.has_value()
                                          ? *params.arguments
                                          : emptyArguments;
              return adapt_result<::lsp::Workspace_ExecuteCommandResult>(
                  server,
                  executeCommand(sharedServices, params.command, arguments,
                                 cancelToken),
                  wrap_optional_payload<::lsp::Workspace_ExecuteCommandResult>{},
                  cancelToken);
            }));
  }

  add_request_handler<::lsp::requests::Workspace_Symbol>(
      handler,
      create_server_request_handler<::lsp::requests::Workspace_Symbol>(
          server, sharedServices, workspaceSymbolRequirement,
          [&sharedServices](const ::lsp::WorkspaceSymbolParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getWorkspaceSymbols(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::Workspace_SymbolResult>{}));

  if (sharedServices.lsp.workspaceSymbolProvider != nullptr &&
      sharedServices.lsp.workspaceSymbolProvider->supportsResolveSymbol()) {
    add_request_handler<::lsp::requests::WorkspaceSymbol_Resolve>(
        handler,
        make_async_request<::lsp::requests::WorkspaceSymbol_Resolve>(
            server,
            [&server, &sharedServices,
             workspaceSymbolRequirement](::lsp::WorkspaceSymbol &&symbol,
                                         const utils::CancellationToken &cancelToken) {
              ensure_initialized(server);
              wait_until_phase(sharedServices, cancelToken, std::nullopt,
                               workspaceSymbolRequirement);
              auto symbolForResolve = symbol;
              auto resolvedSymbol =
                  resolveWorkspaceSymbol(sharedServices, symbolForResolve,
                                         cancelToken);
              return adapt_result<::lsp::WorkspaceSymbol>(
                  server, std::move(resolvedSymbol),
                  wrap_resolved_or_original<::lsp::WorkspaceSymbol>{
                      std::move(symbol)},
                  cancelToken);
            }));
  }
}

} // namespace pegium
