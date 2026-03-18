#include <pegium/lsp/LanguageServerFeatures.hpp>

#include <utility>

#include <pegium/lsp/ServiceAccess.hpp>

namespace pegium::lsp {

namespace {

template <typename F>
decltype(auto) with_workspace_read_lock(
    const services::SharedServices &sharedServices, F &&action) {
  return workspace::run_with_workspace_read(
      sharedServices.workspace.workspaceLock.get(), std::forward<F>(action));
}

template <typename Task>
auto read_workspace_request(services::SharedServices &sharedServices, Task &&task)
    -> std::invoke_result_t<Task> {
  return with_workspace_read_lock(sharedServices, std::forward<Task>(task));
}

template <typename Task>
auto read_document_request(services::SharedServices &sharedServices,
                           std::string uri, Task &&task)
    -> std::invoke_result_t<Task, const workspace::Document &> {
  using Result = std::invoke_result_t<Task, const workspace::Document &>;
  return with_workspace_read_lock(
      sharedServices,
      [&sharedServices, uri = std::move(uri), task = std::forward<Task>(task)]()
          mutable -> Result {
        if (sharedServices.workspace.documents == nullptr) {
          return Result{};
        }
        auto document = sharedServices.workspace.documents->getDocument(uri);
        if (document == nullptr) {
          return Result{};
        }
        return task(*document);
      });
}

template <typename Result, typename Accessor, typename Invoker>
Result with_document_provider(services::SharedServices &sharedServices,
                              std::string uri, Accessor accessor,
                              Invoker invoker) {
  return read_document_request(
      sharedServices, std::move(uri),
      [&sharedServices, &accessor,
       &invoker](const workspace::Document &document) -> Result {
        if (sharedServices.serviceRegistry == nullptr) {
          return Result{};
        }
        const auto *languageServices =
            get_services(sharedServices.serviceRegistry.get(), document.languageId);
        const auto *provider =
            languageServices != nullptr ? accessor(*languageServices) : nullptr;
        if (provider == nullptr) {
          return Result{};
        }
        return invoker(*provider, document);
      });
}

template <typename Result, typename Accessor, typename Invoker>
Result with_item_provider(services::SharedServices &sharedServices,
                          const std::string &uri, Accessor accessor,
                          Invoker invoker) {
  return read_workspace_request(
      sharedServices, [&sharedServices, &uri, &accessor, &invoker]() -> Result {
        if (sharedServices.workspace.documents == nullptr ||
            sharedServices.serviceRegistry == nullptr) {
          return Result{};
        }
        auto document = sharedServices.workspace.documents->getDocument(uri);
        if (document == nullptr) {
          return Result{};
        }
        const auto *languageServices =
            get_services(sharedServices.serviceRegistry.get(), document->languageId);
        const auto *provider =
            languageServices != nullptr ? accessor(*languageServices) : nullptr;
        if (provider == nullptr) {
          return Result{};
        }
        return invoker(*provider);
      });
}

} // namespace

std::optional<::lsp::CompletionList>
getCompletion(services::SharedServices &sharedServices,
              const ::lsp::CompletionParams &params,
              const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::CompletionList>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.completionProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getCompletion(document, params, cancelToken);
      });
}

std::optional<::lsp::SignatureHelp>
getSignatureHelp(services::SharedServices &sharedServices,
                 const ::lsp::SignatureHelpParams &params,
                 const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::SignatureHelp>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.signatureHelp.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.provideSignatureHelp(document, params, cancelToken);
      });
}

std::optional<::lsp::Hover>
getHoverContent(services::SharedServices &sharedServices,
                const ::lsp::HoverParams &params,
                const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::Hover>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.hoverProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getHoverContent(document, params, cancelToken);
      });
}

std::vector<::lsp::CodeLens>
getCodeLens(services::SharedServices &sharedServices,
            const ::lsp::CodeLensParams &params,
            const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::CodeLens>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.codeLensProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.provideCodeLens(document, params, cancelToken);
      });
}

std::vector<::lsp::DocumentSymbol>
getDocumentSymbols(services::SharedServices &sharedServices,
                   const ::lsp::DocumentSymbolParams &params,
                   const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::DocumentSymbol>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.documentSymbolProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getSymbols(document, params, cancelToken);
      });
}

std::vector<::lsp::DocumentHighlight>
getDocumentHighlights(services::SharedServices &sharedServices,
                      const ::lsp::DocumentHighlightParams &params,
                      const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::DocumentHighlight>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.documentHighlightProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getDocumentHighlight(document, params, cancelToken);
      });
}

std::vector<::lsp::FoldingRange>
getFoldingRanges(services::SharedServices &sharedServices,
                 const ::lsp::FoldingRangeParams &params,
                 const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::FoldingRange>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.foldingRangeProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getFoldingRanges(document, params, cancelToken);
      });
}

std::optional<std::vector<::lsp::LocationLink>>
getDeclaration(services::SharedServices &sharedServices,
               const ::lsp::DeclarationParams &params,
               const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<std::vector<::lsp::LocationLink>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.declarationProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getDeclaration(document, params, cancelToken);
      });
}

std::optional<std::vector<::lsp::LocationLink>>
getDefinition(services::SharedServices &sharedServices,
              const ::lsp::DefinitionParams &params,
              const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<std::vector<::lsp::LocationLink>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.definitionProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getDefinition(document, params, cancelToken);
      });
}

std::optional<std::vector<::lsp::LocationLink>>
getTypeDefinition(services::SharedServices &sharedServices,
                  const ::lsp::TypeDefinitionParams &params,
                  const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<std::vector<::lsp::LocationLink>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.typeProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getTypeDefinition(document, params, cancelToken);
      });
}

std::optional<std::vector<::lsp::LocationLink>>
getImplementation(services::SharedServices &sharedServices,
                  const ::lsp::ImplementationParams &params,
                  const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<std::vector<::lsp::LocationLink>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.implementationProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getImplementation(document, params, cancelToken);
      });
}

std::vector<::lsp::Location>
getReferences(services::SharedServices &sharedServices,
              const ::lsp::ReferenceParams &params,
              const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::Location>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.referencesProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.findReferences(document, params, cancelToken);
      });
}

std::optional<::lsp::WorkspaceEdit>
rename(services::SharedServices &sharedServices,
       const ::lsp::RenameParams &params,
       const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::WorkspaceEdit>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.renameProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.rename(document, params, cancelToken);
      });
}

std::vector<::lsp::TextEdit>
formatDocument(services::SharedServices &sharedServices,
               const ::lsp::DocumentFormattingParams &params,
               const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::TextEdit>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.formatter.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.formatDocument(document, params, cancelToken);
      });
}

std::vector<::lsp::TextEdit>
formatDocumentRange(services::SharedServices &sharedServices,
                    const ::lsp::DocumentRangeFormattingParams &params,
                    const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::TextEdit>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.formatter.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.formatDocumentRange(document, params, cancelToken);
      });
}

std::vector<::lsp::TextEdit>
formatDocumentOnType(services::SharedServices &sharedServices,
                     const ::lsp::DocumentOnTypeFormattingParams &params,
                     const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::TextEdit>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.formatter.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.formatDocumentOnType(document, params, cancelToken);
      });
}

std::vector<::lsp::InlayHint>
getInlayHints(services::SharedServices &sharedServices,
              const ::lsp::InlayHintParams &params,
              const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::InlayHint>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.inlayHintProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getInlayHints(document, params, cancelToken);
      });
}

std::optional<::lsp::SemanticTokens>
getSemanticTokensFull(services::SharedServices &sharedServices,
                      const ::lsp::SemanticTokensParams &params,
                      const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::SemanticTokens>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.semanticTokenProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.semanticHighlight(document, params, cancelToken);
      });
}

std::optional<::lsp::SemanticTokens>
getSemanticTokensRange(services::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensRangeParams &params,
                       const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::SemanticTokens>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.semanticTokenProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.semanticHighlightRange(document, params, cancelToken);
      });
}

std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>
getSemanticTokensDelta(services::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensDeltaParams &params,
                       const utils::CancellationToken &cancelToken) {
  return with_document_provider<
      std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.semanticTokenProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.semanticHighlightDelta(document, params, cancelToken);
      });
}

std::vector<std::string>
getExecuteCommands(const services::SharedServices &sharedServices) {
  if (sharedServices.lsp.executeCommandHandler == nullptr) {
    return {};
  }
  return sharedServices.lsp.executeCommandHandler->commands();
}

std::optional<::lsp::LSPAny>
executeCommand(services::SharedServices &sharedServices,
               std::string_view name, const ::lsp::LSPArray &arguments,
               const utils::CancellationToken &cancelToken) {
  if (sharedServices.lsp.executeCommandHandler == nullptr) {
    return std::nullopt;
  }
  return sharedServices.lsp.executeCommandHandler->executeCommand(
      name, arguments, cancelToken);
}

std::vector<::lsp::CallHierarchyItem>
prepareCallHierarchy(services::SharedServices &sharedServices,
                     const ::lsp::CallHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::CallHierarchyItem>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.callHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.prepareCallHierarchy(document, params, cancelToken);
      });
}

std::vector<::lsp::CallHierarchyIncomingCall>
getIncomingCalls(services::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyIncomingCallsParams &params,
                 const utils::CancellationToken &cancelToken) {
  return with_item_provider<std::vector<::lsp::CallHierarchyIncomingCall>>(
      sharedServices, params.item.uri.toString(),
      [](const auto &services) {
        return services.lsp.callHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider) {
        return provider.incomingCalls(params, cancelToken);
      });
}

std::vector<::lsp::CallHierarchyOutgoingCall>
getOutgoingCalls(services::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyOutgoingCallsParams &params,
                 const utils::CancellationToken &cancelToken) {
  return with_item_provider<std::vector<::lsp::CallHierarchyOutgoingCall>>(
      sharedServices, params.item.uri.toString(),
      [](const auto &services) {
        return services.lsp.callHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider) {
        return provider.outgoingCalls(params, cancelToken);
      });
}

std::vector<::lsp::TypeHierarchyItem>
prepareTypeHierarchy(services::SharedServices &sharedServices,
                     const ::lsp::TypeHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::TypeHierarchyItem>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.typeHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.prepareTypeHierarchy(document, params, cancelToken);
      });
}

std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySupertypes(services::SharedServices &sharedServices,
                           const ::lsp::TypeHierarchySupertypesParams &params,
                           const utils::CancellationToken &cancelToken) {
  return with_item_provider<std::vector<::lsp::TypeHierarchyItem>>(
      sharedServices, params.item.uri.toString(),
      [](const auto &services) {
        return services.lsp.typeHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider) {
        return provider.supertypes(params, cancelToken);
      });
}

std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySubtypes(services::SharedServices &sharedServices,
                         const ::lsp::TypeHierarchySubtypesParams &params,
                         const utils::CancellationToken &cancelToken) {
  return with_item_provider<std::vector<::lsp::TypeHierarchyItem>>(
      sharedServices, params.item.uri.toString(),
      [](const auto &services) {
        return services.lsp.typeHierarchyProvider.get();
      },
      [&params, &cancelToken](const auto &provider) {
        return provider.subtypes(params, cancelToken);
      });
}

std::optional<std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
getCodeActions(services::SharedServices &sharedServices,
               const ::lsp::CodeActionParams &params,
               const utils::CancellationToken &cancelToken) {
  return with_document_provider<
      std::optional<std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.codeActionProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getCodeActions(document, params, cancelToken);
      });
}

std::vector<::lsp::DocumentLink>
getDocumentLinks(services::SharedServices &sharedServices,
                 const ::lsp::DocumentLinkParams &params,
                 const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::DocumentLink>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.documentLinkProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getDocumentLinks(document, params, cancelToken);
      });
}

std::vector<::lsp::SelectionRange>
getSelectionRanges(services::SharedServices &sharedServices,
                   const ::lsp::SelectionRangeParams &params,
                   const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::SelectionRange>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) {
        return services.lsp.selectionRangeProvider.get();
      },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.getSelectionRanges(document, params, cancelToken);
      });
}

std::optional<::lsp::PrepareRenameResult>
prepareRename(services::SharedServices &sharedServices,
              const ::lsp::PrepareRenameParams &params,
              const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::optional<::lsp::PrepareRenameResult>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.renameProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        return provider.prepareRename(document, params, cancelToken);
      });
}

std::vector<::lsp::WorkspaceSymbol>
getWorkspaceSymbols(services::SharedServices &sharedServices,
                    const ::lsp::WorkspaceSymbolParams &params,
                    const utils::CancellationToken &cancelToken) {
  return read_workspace_request(
      sharedServices,
      [&sharedServices, &params,
       &cancelToken]() -> std::vector<::lsp::WorkspaceSymbol> {
        if (sharedServices.lsp.workspaceSymbolProvider == nullptr) {
          return {};
        }
        return sharedServices.lsp.workspaceSymbolProvider->getSymbols(params,
                                                                      cancelToken);
      });
}

std::optional<::lsp::WorkspaceSymbol>
resolveWorkspaceSymbol(services::SharedServices &sharedServices,
                       const ::lsp::WorkspaceSymbol &symbol,
                       const utils::CancellationToken &cancelToken) {
  return read_workspace_request(
      sharedServices,
      [&sharedServices, &symbol,
       &cancelToken]() -> std::optional<::lsp::WorkspaceSymbol> {
        if (sharedServices.lsp.workspaceSymbolProvider == nullptr ||
            !sharedServices.lsp.workspaceSymbolProvider
                 ->supportsResolveSymbol()) {
          return std::nullopt;
        }
        return sharedServices.lsp.workspaceSymbolProvider->resolveSymbol(
            symbol, cancelToken);
      });
}

} // namespace pegium::lsp
