#pragma once

/// pegium's headless language-server feature API: every LSP feature as a free
/// function over a `SharedServices` and an `lsp::*Params`, returning the LSP
/// result directly — no running server, transport, or message loop. The actual
/// language server (and the `pegium::testing` harness) are just callers of these.
///
/// Use this to drive a feature from a test or tool when the high-level
/// `pegium::testing` `expect…` helpers are not enough (e.g. rename, prepare
/// rename, code actions, or asserting on the raw protocol result). Build the
/// `params` for an offset with `document.textDocument().positionAt(offset)`.
///
/// Thread-safety: every feature function is safe to call concurrently. Each
/// operates on a read-locked snapshot of the workspace (it coordinates with the
/// document-build pipeline), so concurrent feature requests do not race with
/// each other or with in-flight builds. The query functions take a
/// `const SharedServices&` (they only read language state); `executeCommand`
/// takes a non-const reference, since running a command may change it.

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
getCompletion(const pegium::SharedServices &sharedServices,
              const ::lsp::CompletionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Resolves signature help for one cursor position.
[[nodiscard]] std::optional<::lsp::SignatureHelp>
getSignatureHelp(const pegium::SharedServices &sharedServices,
                 const ::lsp::SignatureHelpParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves hover content for one cursor position.
[[nodiscard]] std::optional<::lsp::Hover>
getHoverContent(const pegium::SharedServices &sharedServices,
                const ::lsp::HoverParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token);

/// Computes code lenses for one document.
[[nodiscard]] std::vector<::lsp::CodeLens>
getCodeLens(const pegium::SharedServices &sharedServices,
            const ::lsp::CodeLensParams &params,
            const utils::CancellationToken &cancelToken =
                utils::default_cancel_token);

/// Resolves deferred metadata for one code lens.
[[nodiscard]] std::optional<::lsp::CodeLens>
resolveCodeLens(const pegium::SharedServices &sharedServices,
                const ::lsp::CodeLens &codeLens,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token);

/// Computes document symbols for one document.
[[nodiscard]] std::vector<::lsp::DocumentSymbol>
getDocumentSymbols(const pegium::SharedServices &sharedServices,
                   const ::lsp::DocumentSymbolParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

/// Computes highlight ranges at one cursor position.
[[nodiscard]] std::vector<::lsp::DocumentHighlight>
getDocumentHighlights(const pegium::SharedServices &sharedServices,
                      const ::lsp::DocumentHighlightParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

/// Computes folding ranges for one document.
[[nodiscard]] std::vector<::lsp::FoldingRange>
getFoldingRanges(const pegium::SharedServices &sharedServices,
                 const ::lsp::FoldingRangeParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves declaration targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDeclaration(const pegium::SharedServices &sharedServices,
               const ::lsp::DeclarationParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Resolves definition targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getDefinition(const pegium::SharedServices &sharedServices,
              const ::lsp::DefinitionParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Resolves type-definition targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getTypeDefinition(const pegium::SharedServices &sharedServices,
                  const ::lsp::TypeDefinitionParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

/// Resolves implementation targets for one cursor position.
[[nodiscard]] std::optional<std::vector<::lsp::LocationLink>>
getImplementation(const pegium::SharedServices &sharedServices,
                  const ::lsp::ImplementationParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token);

/// Resolves all references reachable from one cursor position.
[[nodiscard]] std::vector<::lsp::Location>
getReferences(const pegium::SharedServices &sharedServices,
              const ::lsp::ReferenceParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Computes the workspace edit for a rename request.
[[nodiscard]] std::optional<::lsp::WorkspaceEdit>
rename(const pegium::SharedServices &sharedServices,
       const ::lsp::RenameParams &params,
       const utils::CancellationToken &cancelToken =
           utils::default_cancel_token);

/// Formats a whole document.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocument(const pegium::SharedServices &sharedServices,
               const ::lsp::DocumentFormattingParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Formats one document range.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentRange(const pegium::SharedServices &sharedServices,
                    const ::lsp::DocumentRangeFormattingParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

/// Formats one document after a trigger character was typed.
[[nodiscard]] std::vector<::lsp::TextEdit>
formatDocumentOnType(const pegium::SharedServices &sharedServices,
                     const ::lsp::DocumentOnTypeFormattingParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Computes inlay hints for one document range.
[[nodiscard]] std::vector<::lsp::InlayHint>
getInlayHints(const pegium::SharedServices &sharedServices,
              const ::lsp::InlayHintParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Computes a full semantic-token snapshot.
[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensFull(const pegium::SharedServices &sharedServices,
                      const ::lsp::SemanticTokensParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token);

/// Computes semantic tokens for one document range.
[[nodiscard]] std::optional<::lsp::SemanticTokens>
getSemanticTokensRange(const pegium::SharedServices &sharedServices,
                       const ::lsp::SemanticTokensRangeParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

/// Computes a semantic-token delta from a previous result id.
[[nodiscard]] std::optional<::lsp::OneOf<::lsp::SemanticTokens, ::lsp::SemanticTokensDelta>>
getSemanticTokensDelta(const pegium::SharedServices &sharedServices,
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
prepareCallHierarchy(const pegium::SharedServices &sharedServices,
                     const ::lsp::CallHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Resolves incoming calls for one call-hierarchy item.
[[nodiscard]] std::vector<::lsp::CallHierarchyIncomingCall>
getIncomingCalls(const pegium::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyIncomingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves outgoing calls for one call-hierarchy item.
[[nodiscard]] std::vector<::lsp::CallHierarchyOutgoingCall>
getOutgoingCalls(const pegium::SharedServices &sharedServices,
                 const ::lsp::CallHierarchyOutgoingCallsParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Resolves the initial type-hierarchy items at one cursor position.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
prepareTypeHierarchy(const pegium::SharedServices &sharedServices,
                     const ::lsp::TypeHierarchyPrepareParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token);

/// Resolves direct supertypes for one type-hierarchy item.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySupertypes(const pegium::SharedServices &sharedServices,
                           const ::lsp::TypeHierarchySupertypesParams &params,
                           const utils::CancellationToken &cancelToken =
                               utils::default_cancel_token);

/// Resolves direct subtypes for one type-hierarchy item.
[[nodiscard]] std::vector<::lsp::TypeHierarchyItem>
getTypeHierarchySubtypes(const pegium::SharedServices &sharedServices,
                         const ::lsp::TypeHierarchySubtypesParams &params,
                         const utils::CancellationToken &cancelToken =
                             utils::default_cancel_token);

/// Computes code actions for one document range.
[[nodiscard]] std::optional<std::vector<
    ::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
getCodeActions(const pegium::SharedServices &sharedServices,
               const ::lsp::CodeActionParams &params,
               const utils::CancellationToken &cancelToken =
                   utils::default_cancel_token);

/// Resolves document links for one document.
[[nodiscard]] std::vector<::lsp::DocumentLink>
getDocumentLinks(const pegium::SharedServices &sharedServices,
                 const ::lsp::DocumentLinkParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token);

/// Computes selection ranges for one or more cursor positions.
[[nodiscard]] std::vector<::lsp::SelectionRange>
getSelectionRanges(const pegium::SharedServices &sharedServices,
                   const ::lsp::SelectionRangeParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token);

/// Checks whether the symbol at the cursor can be renamed.
[[nodiscard]] std::optional<::lsp::PrepareRenameResult>
prepareRename(const pegium::SharedServices &sharedServices,
              const ::lsp::PrepareRenameParams &params,
              const utils::CancellationToken &cancelToken =
                  utils::default_cancel_token);

/// Searches workspace symbols matching `params`.
[[nodiscard]] std::vector<::lsp::WorkspaceSymbol>
getWorkspaceSymbols(const pegium::SharedServices &sharedServices,
                    const ::lsp::WorkspaceSymbolParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token);

/// Resolves deferred metadata for one workspace symbol.
[[nodiscard]] std::optional<::lsp::WorkspaceSymbol>
resolveWorkspaceSymbol(const pegium::SharedServices &sharedServices,
                       const ::lsp::WorkspaceSymbol &symbol,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token);

} // namespace pegium
