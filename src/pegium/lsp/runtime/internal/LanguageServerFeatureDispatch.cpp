#include <pegium/lsp/runtime/internal/LanguageServerFeatureDispatch.hpp>

#include <cassert>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include <lsp/error.h>

#include <pegium/lsp/services/ServiceAccess.hpp>
#include <pegium/lsp/support/JsonValue.hpp>

namespace pegium {

namespace {

constexpr std::string_view kCodeLensMetadataKey = "__pegiumCodeLens";
constexpr std::string_view kCodeLensUriKey = "uri";
constexpr std::string_view kCodeLensDataKey = "data";

struct WrappedCodeLens {
  std::string uri;
  ::lsp::CodeLens codeLens;
};

template <typename F>
decltype(auto) with_workspace_read_lock(
    const pegium::SharedServices &sharedServices, F &&action) {
  auto *lock = sharedServices.workspace.workspaceLock.get();
  assert(lock != nullptr);

  using Result = std::invoke_result_t<F>;
  if constexpr (std::is_void_v<Result>) {
    lock->read([task = std::forward<F>(action)]() mutable { task(); }).get();
    return;
  } else {
    std::optional<Result> result;
    lock->read([&result, task = std::forward<F>(action)]() mutable {
      result.emplace(task());
    }).get();
    Result value = std::move(result).value();
    return value;
  }
}

template <typename Result, typename Accessor, typename Invoker>
Result with_document_provider(pegium::SharedServices &sharedServices,
                              std::string uri, Accessor accessor,
                              Invoker invoker) {
  return with_workspace_read_lock(
      sharedServices,
      [&sharedServices, uri = std::move(uri), &accessor,
       &invoker]() -> Result {
        auto document = sharedServices.workspace.documents->getDocument(uri);
        if (document == nullptr) {
          return Result{};
        }
        const auto *services =
            get_services(*sharedServices.serviceRegistry, document->uri);
        assert(services != nullptr);
        const auto *provider = accessor(*services);
        if (provider == nullptr) {
          return Result{};
        }
        return invoker(*provider, *document);
      });
}

template <typename Result, typename Accessor, typename Invoker>
Result with_item_provider(pegium::SharedServices &sharedServices,
                          const std::string &uri, Accessor accessor,
                          Invoker invoker) {
  return with_workspace_read_lock(
      sharedServices, [&sharedServices, &uri, &accessor, &invoker]() -> Result {
        const auto *services = get_services(*sharedServices.serviceRegistry, uri);
        if (services == nullptr) {
          throw ::lsp::RequestError(
              ::lsp::MessageError::RequestFailed,
              "Could not find service instance for uri: '" + uri + "'");
        }
        const auto *provider = accessor(*services);
        if (provider == nullptr) {
          return Result{};
        }
        return invoker(*provider);
      });
}

::lsp::CodeLens wrap_code_lens(std::string_view uri, ::lsp::CodeLens codeLens) {
  pegium::JsonValue::Object metadata;
  metadata.try_emplace(std::string(kCodeLensMetadataKey), true);
  metadata.try_emplace(std::string(kCodeLensUriKey), std::string(uri));
  if (codeLens.data.has_value()) {
    metadata.try_emplace(std::string(kCodeLensDataKey),
                         from_lsp_any(*codeLens.data));
  }
  codeLens.data = to_lsp_any(pegium::JsonValue(std::move(metadata)));
  return codeLens;
}

std::optional<WrappedCodeLens> unwrap_code_lens(const ::lsp::CodeLens &codeLens) {
  if (!codeLens.data.has_value()) {
    return std::nullopt;
  }

  const auto data = from_lsp_any(*codeLens.data);
  if (!data.isObject()) {
    return std::nullopt;
  }

  const auto &metadata = data.object();
  const auto markerIt = metadata.find(kCodeLensMetadataKey);
  if (markerIt == metadata.end() || !markerIt->second.isBoolean() ||
      !markerIt->second.boolean()) {
    return std::nullopt;
  }

  const auto uriIt = metadata.find(kCodeLensUriKey);
  if (uriIt == metadata.end() || !uriIt->second.isString()) {
    return std::nullopt;
  }

  ::lsp::CodeLens unwrapped = codeLens;
  if (const auto dataIt = metadata.find(kCodeLensDataKey);
      dataIt != metadata.end()) {
    unwrapped.data = to_lsp_any(dataIt->second);
  } else {
    unwrapped.data = std::nullopt;
  }

  return WrappedCodeLens{.uri = uriIt->second.string(),
                         .codeLens = std::move(unwrapped)};
}

} // namespace

std::optional<::lsp::CompletionList>
getCompletion(pegium::SharedServices &sharedServices,
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
getSignatureHelp(pegium::SharedServices &sharedServices,
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
getHoverContent(pegium::SharedServices &sharedServices,
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
getCodeLens(pegium::SharedServices &sharedServices,
            const ::lsp::CodeLensParams &params,
            const utils::CancellationToken &cancelToken) {
  return with_document_provider<std::vector<::lsp::CodeLens>>(
      sharedServices, params.textDocument.uri.toString(),
      [](const auto &services) { return services.lsp.codeLensProvider.get(); },
      [&params, &cancelToken](const auto &provider,
                              const workspace::Document &document) {
        auto codeLens = provider.provideCodeLens(document, params, cancelToken);
        if (!provider.supportsResolveCodeLens()) {
          return codeLens;
        }
        for (auto &item : codeLens) {
          utils::throw_if_cancelled(cancelToken);
          item = wrap_code_lens(document.uri, std::move(item));
        }
        return codeLens;
      });
}

std::optional<::lsp::CodeLens>
resolveCodeLens(pegium::SharedServices &sharedServices,
                const ::lsp::CodeLens &codeLens,
                const utils::CancellationToken &cancelToken) {
  return with_workspace_read_lock(
      sharedServices,
      [&sharedServices, &codeLens,
       &cancelToken]() -> std::optional<::lsp::CodeLens> {
        auto wrappedCodeLens = unwrap_code_lens(codeLens);
        if (!wrappedCodeLens.has_value()) {
          return std::nullopt;
        }

        const auto *services =
            get_services(*sharedServices.serviceRegistry, wrappedCodeLens->uri);
        if (services == nullptr) {
          throw ::lsp::RequestError(
              ::lsp::MessageError::RequestFailed,
              "Could not find service instance for uri: '" +
                  wrappedCodeLens->uri + "'");
        }

        const auto *provider = services->lsp.codeLensProvider.get();
        if (provider == nullptr || !provider->supportsResolveCodeLens()) {
          return std::nullopt;
        }

        auto resolved =
            provider->resolveCodeLens(wrappedCodeLens->codeLens, cancelToken);
        if (!resolved.has_value()) {
          return std::nullopt;
        }
        if (!resolved->data.has_value() &&
            wrappedCodeLens->codeLens.data.has_value()) {
          resolved->data = wrappedCodeLens->codeLens.data;
        }
        return wrap_code_lens(wrappedCodeLens->uri, std::move(*resolved));
      });
}

std::vector<::lsp::DocumentSymbol>
getDocumentSymbols(pegium::SharedServices &sharedServices,
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
getDocumentHighlights(pegium::SharedServices &sharedServices,
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
getFoldingRanges(pegium::SharedServices &sharedServices,
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
getDeclaration(pegium::SharedServices &sharedServices,
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
getDefinition(pegium::SharedServices &sharedServices,
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
getTypeDefinition(pegium::SharedServices &sharedServices,
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
getImplementation(pegium::SharedServices &sharedServices,
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
getReferences(pegium::SharedServices &sharedServices,
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
rename(pegium::SharedServices &sharedServices,
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
formatDocument(pegium::SharedServices &sharedServices,
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
formatDocumentRange(pegium::SharedServices &sharedServices,
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
formatDocumentOnType(pegium::SharedServices &sharedServices,
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
getInlayHints(pegium::SharedServices &sharedServices,
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
getSemanticTokensFull(pegium::SharedServices &sharedServices,
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
getSemanticTokensRange(pegium::SharedServices &sharedServices,
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
getSemanticTokensDelta(pegium::SharedServices &sharedServices,
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
getExecuteCommands(const pegium::SharedServices &sharedServices) {
  if (sharedServices.lsp.executeCommandHandler == nullptr) {
    return {};
  }
  return sharedServices.lsp.executeCommandHandler->commands();
}

std::optional<::lsp::LSPAny>
executeCommand(pegium::SharedServices &sharedServices,
               std::string_view name, const ::lsp::LSPArray &arguments,
               const utils::CancellationToken &cancelToken) {
  if (sharedServices.lsp.executeCommandHandler == nullptr) {
    return std::nullopt;
  }
  return sharedServices.lsp.executeCommandHandler->executeCommand(
      name, arguments, cancelToken);
}

std::vector<::lsp::CallHierarchyItem>
prepareCallHierarchy(pegium::SharedServices &sharedServices,
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
getIncomingCalls(pegium::SharedServices &sharedServices,
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
getOutgoingCalls(pegium::SharedServices &sharedServices,
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
prepareTypeHierarchy(pegium::SharedServices &sharedServices,
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
getTypeHierarchySupertypes(pegium::SharedServices &sharedServices,
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
getTypeHierarchySubtypes(pegium::SharedServices &sharedServices,
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
getCodeActions(pegium::SharedServices &sharedServices,
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
getDocumentLinks(pegium::SharedServices &sharedServices,
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
getSelectionRanges(pegium::SharedServices &sharedServices,
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
prepareRename(pegium::SharedServices &sharedServices,
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
getWorkspaceSymbols(pegium::SharedServices &sharedServices,
                    const ::lsp::WorkspaceSymbolParams &params,
                    const utils::CancellationToken &cancelToken) {
  return with_workspace_read_lock(
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
resolveWorkspaceSymbol(pegium::SharedServices &sharedServices,
                       const ::lsp::WorkspaceSymbol &symbol,
                       const utils::CancellationToken &cancelToken) {
  return with_workspace_read_lock(
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

} // namespace pegium
