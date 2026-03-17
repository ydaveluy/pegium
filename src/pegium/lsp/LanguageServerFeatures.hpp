#pragma once

#include <optional>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/services/Services.hpp>
#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Cancellation.hpp>

namespace pegium::lsp {

[[nodiscard]] std::optional<::lsp::CompletionList>
getCompletion(services::SharedServices &sharedServices,
              const ::lsp::CompletionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::SignatureHelp>
getSignatureHelp(services::SharedServices &sharedServices,
                 const ::lsp::SignatureHelpParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::Hover>
getHoverContent(services::SharedServices &sharedServices,
                const ::lsp::HoverParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::CodeLens>
getCodeLens(services::SharedServices &sharedServices,
            const ::lsp::CodeLensParams &params,
            const utils::CancellationToken &cancelToken =
                utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::DocumentSymbol>
getDocumentSymbols(services::SharedServices &sharedServices,
                   const ::lsp::DocumentSymbolParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::DocumentHighlight>
getDocumentHighlights(services::SharedServices &sharedServices,
                      const ::lsp::DocumentHighlightParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::FoldingRange>
getFoldingRanges(services::SharedServices &sharedServices,
                 const ::lsp::FoldingRangeParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDeclaration(services::SharedServices &sharedServices,
               const ::lsp::DeclarationParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDefinition(services::SharedServices &sharedServices,
              const ::lsp::DefinitionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getTypeDefinition(services::SharedServices &sharedServices,
                  const ::lsp::TypeDefinitionParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getImplementation(services::SharedServices &sharedServices,
                  const ::lsp::ImplementationParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::Location>
getReferences(services::SharedServices &sharedServices,
              const ::lsp::ReferenceParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::WorkspaceEdit>
rename(services::SharedServices &sharedServices,
       const ::lsp::RenameParams &params,
       const utils::CancellationToken &cancelToken =
           utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocument(services::SharedServices &sharedServices,
               const ::lsp::DocumentFormattingParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentRange(services::SharedServices &sharedServices,
                    const ::lsp::DocumentRangeFormattingParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentOnType(services::SharedServices &sharedServices,
                     const ::lsp::DocumentOnTypeFormattingParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::InlayHint>
getInlayHints(services::SharedServices &sharedServices,
              const ::lsp::InlayHintParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensFull(services::SharedServices &sharedServices,
                      const ::lsp::SemanticTokensParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensRange(services::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensRangeParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>
getSemanticTokensDelta(services::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensDeltaParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

[[nodiscard]] std::vector<std::string>
getExecuteCommands(const services::SharedServices &sharedServices);

[[nodiscard]] std::optional<::lsp::LSPAny>
executeCommand(services::SharedServices &sharedServices,
               std::string_view name, const ::lsp::LSPArray &arguments,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::CallHierarchyItem>
prepareCallHierarchy(services::SharedServices &sharedServices,
                     const ::lsp::CallHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::CallHierarchyIncomingCall>
getIncomingCalls(services::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyIncomingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::CallHierarchyOutgoingCall>
getOutgoingCalls(services::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyOutgoingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
prepareTypeHierarchy(services::SharedServices &sharedServices,
                     const ::lsp::TypeHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySupertypes(services::SharedServices &sharedServices,
                           const ::lsp::TypeHierarchySupertypesParams &params,
                           const utils::CancellationToken &cancelToken =
                               utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySubtypes(services::SharedServices &sharedServices,
                         const ::lsp::TypeHierarchySubtypesParams &params,
                         const utils::CancellationToken &cancelToken =
                             utils::default_cancel_token);

[[nodiscard]] std::optional<std::vector<
    ::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
getCodeActions(services::SharedServices &sharedServices,
               const ::lsp::CodeActionParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::DocumentLink>
getDocumentLinks(services::SharedServices &sharedServices,
                 const ::lsp::DocumentLinkParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::SelectionRange>
getSelectionRanges(services::SharedServices &sharedServices,
                   const ::lsp::SelectionRangeParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::PrepareRenameResult>
prepareRename(services::SharedServices &sharedServices,
              const ::lsp::PrepareRenameParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

[[nodiscard]] std::vector<::lsp::WorkspaceSymbol>
getWorkspaceSymbols(services::SharedServices &sharedServices,
                    const ::lsp::WorkspaceSymbolParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

[[nodiscard]] std::optional<::lsp::WorkspaceSymbol>
resolveWorkspaceSymbol(services::SharedServices &sharedServices,
                       const ::lsp::WorkspaceSymbol &symbol,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

} // namespace pegium::lsp
