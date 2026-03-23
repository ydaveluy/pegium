#pragma once

#include <optional>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/lsp/services/Services.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>

namespace pegium {

/// Resolves completion items for one cursor position.
[[nodiscard]] std::optional<::lsp::CompletionList>
getCompletion(pegium::SharedServices &sharedServices,
              const ::lsp::CompletionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Resolves signature help for one cursor position.
[[nodiscard]] std::optional<::lsp::SignatureHelp>
getSignatureHelp(pegium::SharedServices &sharedServices,
                 const ::lsp::SignatureHelpParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves hover content for one cursor position.
[[nodiscard]] std::optional<::lsp::Hover>
getHoverContent(pegium::SharedServices &sharedServices,
                const ::lsp::HoverParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token);

/// Computes code lenses for one document.
[[nodiscard]] std::vector<::lsp::CodeLens>
getCodeLens(pegium::SharedServices &sharedServices,
            const ::lsp::CodeLensParams &params,
            const utils::CancellationToken &cancelToken =
                utils::default_cancel_token);

/// Resolves deferred metadata for one code lens.
[[nodiscard]] std::optional<::lsp::CodeLens>
resolveCodeLens(pegium::SharedServices &sharedServices,
                const ::lsp::CodeLens &codeLens,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token);

/// Computes document symbols for one document.
[[nodiscard]] std::vector<::lsp::DocumentSymbol>
getDocumentSymbols(pegium::SharedServices &sharedServices,
                   const ::lsp::DocumentSymbolParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

/// Computes highlight ranges at one cursor position.
[[nodiscard]] std::vector<::lsp::DocumentHighlight>
getDocumentHighlights(pegium::SharedServices &sharedServices,
                      const ::lsp::DocumentHighlightParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

/// Computes folding ranges for one document.
[[nodiscard]] std::vector<::lsp::FoldingRange>
getFoldingRanges(pegium::SharedServices &sharedServices,
                 const ::lsp::FoldingRangeParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves declaration targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDeclaration(pegium::SharedServices &sharedServices,
               const ::lsp::DeclarationParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Resolves definition targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDefinition(pegium::SharedServices &sharedServices,
              const ::lsp::DefinitionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Resolves type-definition targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getTypeDefinition(pegium::SharedServices &sharedServices,
                  const ::lsp::TypeDefinitionParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

/// Resolves implementation targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getImplementation(pegium::SharedServices &sharedServices,
                  const ::lsp::ImplementationParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

/// Resolves all references reachable from one cursor position.
[[nodiscard]] std::vector<::lsp::Location>
getReferences(pegium::SharedServices &sharedServices,
              const ::lsp::ReferenceParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Computes the workspace edit for a rename request.
[[nodiscard]] std::optional<::lsp::WorkspaceEdit>
rename(pegium::SharedServices &sharedServices,
       const ::lsp::RenameParams &params,
       const utils::CancellationToken &cancelToken =
           utils::default_cancel_token);

/// Formats a whole document.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocument(pegium::SharedServices &sharedServices,
               const ::lsp::DocumentFormattingParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Formats one document range.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentRange(pegium::SharedServices &sharedServices,
                    const ::lsp::DocumentRangeFormattingParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

/// Formats one document after a trigger character was typed.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentOnType(pegium::SharedServices &sharedServices,
                     const ::lsp::DocumentOnTypeFormattingParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Computes inlay hints for one document range.
[[nodiscard]] std::vector<::lsp::InlayHint>
getInlayHints(pegium::SharedServices &sharedServices,
              const ::lsp::InlayHintParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Computes a full semantic-token snapshot.
[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensFull(pegium::SharedServices &sharedServices,
                      const ::lsp::SemanticTokensParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

/// Computes semantic tokens for one document range.
[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensRange(pegium::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensRangeParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

/// Computes a semantic-token delta from a previous result id.
[[nodiscard]] std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>
getSemanticTokensDelta(pegium::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensDeltaParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

/// Lists executable command identifiers exposed by the language.
[[nodiscard]] std::vector<std::string>
getExecuteCommands(const pegium::SharedServices &sharedServices);

/// Executes one workspace command.
[[nodiscard]] std::optional<::lsp::LSPAny>
executeCommand(pegium::SharedServices &sharedServices,
               std::string_view name, const ::lsp::LSPArray &arguments,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Resolves the initial call-hierarchy items at one cursor position.
[[nodiscard]] std::vector<::lsp::CallHierarchyItem>
prepareCallHierarchy(pegium::SharedServices &sharedServices,
                     const ::lsp::CallHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Resolves incoming calls for one call-hierarchy item.
[[nodiscard]] std::vector<::lsp::CallHierarchyIncomingCall>
getIncomingCalls(pegium::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyIncomingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves outgoing calls for one call-hierarchy item.
[[nodiscard]] std::vector<::lsp::CallHierarchyOutgoingCall>
getOutgoingCalls(pegium::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyOutgoingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves the initial type-hierarchy items at one cursor position.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
prepareTypeHierarchy(pegium::SharedServices &sharedServices,
                     const ::lsp::TypeHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Resolves direct supertypes for one type-hierarchy item.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySupertypes(pegium::SharedServices &sharedServices,
                           const ::lsp::TypeHierarchySupertypesParams &params,
                           const utils::CancellationToken &cancelToken =
                               utils::default_cancel_token);

/// Resolves direct subtypes for one type-hierarchy item.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySubtypes(pegium::SharedServices &sharedServices,
                         const ::lsp::TypeHierarchySubtypesParams &params,
                         const utils::CancellationToken &cancelToken =
                             utils::default_cancel_token);

/// Computes code actions for one document range.
[[nodiscard]] std::optional<std::vector<
    ::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
getCodeActions(pegium::SharedServices &sharedServices,
               const ::lsp::CodeActionParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Resolves document links for one document.
[[nodiscard]] std::vector<::lsp::DocumentLink>
getDocumentLinks(pegium::SharedServices &sharedServices,
                 const ::lsp::DocumentLinkParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Computes selection ranges for one or more cursor positions.
[[nodiscard]] std::vector<::lsp::SelectionRange>
getSelectionRanges(pegium::SharedServices &sharedServices,
                   const ::lsp::SelectionRangeParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

/// Checks whether the symbol at the cursor can be renamed.
[[nodiscard]] std::optional<::lsp::PrepareRenameResult>
prepareRename(pegium::SharedServices &sharedServices,
              const ::lsp::PrepareRenameParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Searches workspace symbols matching `params`.
[[nodiscard]] std::vector<::lsp::WorkspaceSymbol>
getWorkspaceSymbols(pegium::SharedServices &sharedServices,
                    const ::lsp::WorkspaceSymbolParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

/// Resolves deferred metadata for one workspace symbol.
[[nodiscard]] std::optional<::lsp::WorkspaceSymbol>
resolveWorkspaceSymbol(pegium::SharedServices &sharedServices,
                       const ::lsp::WorkspaceSymbol &symbol,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

} // namespace pegium
