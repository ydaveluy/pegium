#include <pegium/lsp/LanguageServerRequestHandlerParts.hpp>

#include <lsp/messagehandler.h>
#include <lsp/messages.h>

#include <pegium/lsp/LanguageServerFeatures.hpp>
#include <pegium/lsp/LanguageServerRequestHandlerUtils.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

void addLanguageServerTextDocumentHandlers(
    LanguageServerHandlerContext &server, ::lsp::MessageHandler &handler,
    services::SharedServices &sharedServices,
    const ServiceRequirements &serviceRequirements) {
  const auto completionRequirement = requirement_or(
      serviceRequirements.CompletionProvider, workspace::DocumentState::Linked);
  const auto signatureHelpRequirement = requirement_or(
      serviceRequirements.SignatureHelp,
      workspace::DocumentState::IndexedReferences);
  const auto hoverRequirement = requirement_or(
      serviceRequirements.HoverProvider, workspace::DocumentState::Linked);
  const auto documentSymbolRequirement = requirement_or(
      serviceRequirements.DocumentSymbolProvider,
      workspace::DocumentState::Parsed);
  const auto codeLensRequirement = requirement_or(
      serviceRequirements.CodeLensProvider,
      workspace::DocumentState::IndexedReferences);
  const auto documentLinkRequirement = requirement_or(
      serviceRequirements.DocumentLinkProvider,
      workspace::DocumentState::Parsed);
  const auto formatterRequirement = requirement_or(
      serviceRequirements.Formatter, workspace::DocumentState::Parsed);
  const auto inlayHintRequirement = requirement_or(
      serviceRequirements.InlayHintProvider,
      workspace::DocumentState::IndexedReferences);
  const auto semanticTokenRequirement = requirement_or(
      serviceRequirements.SemanticTokenProvider,
      workspace::DocumentState::Linked);
  const auto callHierarchyRequirement = requirement_or(
      serviceRequirements.CallHierarchyProvider,
      WorkspaceState::IndexedReferences);
  const auto typeHierarchyRequirement = requirement_or(
      serviceRequirements.TypeHierarchyProvider,
      WorkspaceState::IndexedReferences);
  const auto foldingRangeRequirement = requirement_or(
      serviceRequirements.FoldingRangeProvider,
      workspace::DocumentState::Parsed);
  const auto declarationRequirement = requirement_or(
      serviceRequirements.DeclarationProvider,
      workspace::DocumentState::Linked);
  const auto definitionRequirement = requirement_or(
      serviceRequirements.DefinitionProvider,
      workspace::DocumentState::Linked);
  const auto typeDefinitionRequirement = requirement_or(
      serviceRequirements.TypeProvider, workspace::DocumentState::Linked);
  const auto implementationRequirement = requirement_or(
      serviceRequirements.ImplementationProvider,
      WorkspaceState::IndexedReferences);
  const auto referencesRequirement = requirement_or(
      serviceRequirements.ReferencesProvider,
      WorkspaceState::IndexedReferences);
  const auto documentHighlightRequirement = requirement_or(
      serviceRequirements.DocumentHighlightProvider,
      WorkspaceState::IndexedReferences);
  const auto renameRequirement = requirement_or(
      serviceRequirements.RenameProvider,
      WorkspaceState::IndexedReferences);
  const auto codeActionRequirement = requirement_or(
      serviceRequirements.CodeActionProvider,
      workspace::DocumentState::Validated);
  const auto selectionRangeRequirement =
      ServiceRequirement{workspace::DocumentState::Parsed};

  handler.add<::lsp::requests::TextDocument_Completion>(
      create_request_handler<::lsp::TextDocument_CompletionResult,
                             ::lsp::CompletionParams>(
          server, sharedServices, completionRequirement,
          [&sharedServices](const ::lsp::CompletionParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getCompletion(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_CompletionResult>{}));

  handler.add<::lsp::requests::TextDocument_SignatureHelp>(
      create_request_handler<::lsp::TextDocument_SignatureHelpResult,
                             ::lsp::SignatureHelpParams>(
          server, sharedServices, signatureHelpRequirement,
          [&sharedServices](const ::lsp::SignatureHelpParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getSignatureHelp(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_SignatureHelpResult>{}));

  handler.add<::lsp::requests::TextDocument_Hover>(
      create_request_handler<::lsp::TextDocument_HoverResult,
                             ::lsp::HoverParams>(
          server, sharedServices, hoverRequirement,
          [&sharedServices](const ::lsp::HoverParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getHoverContent(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_HoverResult>{}));

  handler.add<::lsp::requests::TextDocument_DocumentSymbol>(
      create_request_handler<::lsp::TextDocument_DocumentSymbolResult,
                             ::lsp::DocumentSymbolParams>(
          server, sharedServices, documentSymbolRequirement,
          [&sharedServices](const ::lsp::DocumentSymbolParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getDocumentSymbols(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_DocumentSymbolResult>{}));

  handler.add<::lsp::requests::TextDocument_CodeLens>(
      create_request_handler<::lsp::TextDocument_CodeLensResult,
                             ::lsp::CodeLensParams>(
          server, sharedServices, codeLensRequirement,
          [&sharedServices](const ::lsp::CodeLensParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getCodeLens(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_CodeLensResult>{}));

  handler.add<::lsp::requests::TextDocument_DocumentLink>(
      create_request_handler<::lsp::TextDocument_DocumentLinkResult,
                             ::lsp::DocumentLinkParams>(
          server, sharedServices, documentLinkRequirement,
          [&sharedServices](const ::lsp::DocumentLinkParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getDocumentLinks(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_DocumentLinkResult>{}));

  handler.add<::lsp::requests::TextDocument_SelectionRange>(
      create_request_handler<::lsp::TextDocument_SelectionRangeResult,
                             ::lsp::SelectionRangeParams>(
          server, sharedServices, selectionRangeRequirement,
          [&sharedServices](const ::lsp::SelectionRangeParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getSelectionRanges(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_SelectionRangeResult>{}));

  handler.add<::lsp::requests::TextDocument_Formatting>(
      create_request_handler<::lsp::TextDocument_FormattingResult,
                             ::lsp::DocumentFormattingParams>(
          server, sharedServices, formatterRequirement,
          [&sharedServices](const ::lsp::DocumentFormattingParams &params,
                            const utils::CancellationToken &cancelToken) {
            return formatDocument(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_FormattingResult>{}));

  handler.add<::lsp::requests::TextDocument_RangeFormatting>(
      create_request_handler<::lsp::TextDocument_RangeFormattingResult,
                             ::lsp::DocumentRangeFormattingParams>(
          server, sharedServices, formatterRequirement,
          [&sharedServices](
              const ::lsp::DocumentRangeFormattingParams &params,
              const utils::CancellationToken &cancelToken) {
            return formatDocumentRange(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_RangeFormattingResult>{}));

  handler.add<::lsp::requests::TextDocument_OnTypeFormatting>(
      create_request_handler<::lsp::TextDocument_OnTypeFormattingResult,
                             ::lsp::DocumentOnTypeFormattingParams>(
          server, sharedServices, formatterRequirement,
          [&sharedServices](
              const ::lsp::DocumentOnTypeFormattingParams &params,
              const utils::CancellationToken &cancelToken) {
            return formatDocumentOnType(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_OnTypeFormattingResult>{}));

  handler.add<::lsp::requests::TextDocument_InlayHint>(
      create_request_handler<::lsp::TextDocument_InlayHintResult,
                             ::lsp::InlayHintParams>(
          server, sharedServices, inlayHintRequirement,
          [&sharedServices](const ::lsp::InlayHintParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getInlayHints(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_InlayHintResult>{}));

  handler.add<::lsp::requests::TextDocument_SemanticTokens_Full>(
      create_request_handler<::lsp::TextDocument_SemanticTokens_FullResult,
                             ::lsp::SemanticTokensParams>(
          server, sharedServices, semanticTokenRequirement,
          [&sharedServices](const ::lsp::SemanticTokensParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getSemanticTokensFull(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_SemanticTokens_FullResult>{}));

  handler.add<::lsp::requests::TextDocument_SemanticTokens_Full_Delta>(
      create_request_handler<::lsp::TextDocument_SemanticTokens_Full_DeltaResult,
                             ::lsp::SemanticTokensDeltaParams>(
          server, sharedServices, semanticTokenRequirement,
          [&sharedServices](const ::lsp::SemanticTokensDeltaParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getSemanticTokensDelta(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<
              ::lsp::TextDocument_SemanticTokens_Full_DeltaResult>{}));

  handler.add<::lsp::requests::TextDocument_SemanticTokens_Range>(
      create_request_handler<::lsp::TextDocument_SemanticTokens_RangeResult,
                             ::lsp::SemanticTokensRangeParams>(
          server, sharedServices, semanticTokenRequirement,
          [&sharedServices](const ::lsp::SemanticTokensRangeParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getSemanticTokensRange(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<
              ::lsp::TextDocument_SemanticTokens_RangeResult>{}));

  handler.add<::lsp::requests::TextDocument_PrepareCallHierarchy>(
      create_request_handler<::lsp::TextDocument_PrepareCallHierarchyResult,
                             ::lsp::CallHierarchyPrepareParams>(
          server, sharedServices, callHierarchyRequirement,
          [&sharedServices](const ::lsp::CallHierarchyPrepareParams &params,
                            const utils::CancellationToken &cancelToken) {
            return prepareCallHierarchy(sharedServices, params, cancelToken);
          },
          wrap_empty_vector_as_null<
              ::lsp::TextDocument_PrepareCallHierarchyResult>{}));

  handler.add<::lsp::requests::CallHierarchy_IncomingCalls>(
      create_item_request_handler<::lsp::CallHierarchy_IncomingCallsResult,
                                  ::lsp::CallHierarchyIncomingCallsParams>(
          server, sharedServices, callHierarchyRequirement,
          [&sharedServices](
              const ::lsp::CallHierarchyIncomingCallsParams &params,
              const utils::CancellationToken &cancelToken) {
            return getIncomingCalls(sharedServices, params, cancelToken);
          },
          wrap_empty_vector_as_null<::lsp::CallHierarchy_IncomingCallsResult>{}));

  handler.add<::lsp::requests::CallHierarchy_OutgoingCalls>(
      create_item_request_handler<::lsp::CallHierarchy_OutgoingCallsResult,
                                  ::lsp::CallHierarchyOutgoingCallsParams>(
          server, sharedServices, callHierarchyRequirement,
          [&sharedServices](
              const ::lsp::CallHierarchyOutgoingCallsParams &params,
              const utils::CancellationToken &cancelToken) {
            return getOutgoingCalls(sharedServices, params, cancelToken);
          },
          wrap_empty_vector_as_null<::lsp::CallHierarchy_OutgoingCallsResult>{}));

  handler.add<::lsp::requests::TextDocument_PrepareTypeHierarchy>(
      create_request_handler<::lsp::TextDocument_PrepareTypeHierarchyResult,
                             ::lsp::TypeHierarchyPrepareParams>(
          server, sharedServices, typeHierarchyRequirement,
          [&sharedServices](const ::lsp::TypeHierarchyPrepareParams &params,
                            const utils::CancellationToken &cancelToken) {
            return prepareTypeHierarchy(sharedServices, params, cancelToken);
          },
          wrap_empty_vector_as_null<
              ::lsp::TextDocument_PrepareTypeHierarchyResult>{}));

  handler.add<::lsp::requests::TypeHierarchy_Supertypes>(
      create_item_request_handler<::lsp::TypeHierarchy_SupertypesResult,
                                  ::lsp::TypeHierarchySupertypesParams>(
          server, sharedServices, typeHierarchyRequirement,
          [&sharedServices](const ::lsp::TypeHierarchySupertypesParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getTypeHierarchySupertypes(sharedServices, params,
                                              cancelToken);
          },
          wrap_empty_vector_as_null<::lsp::TypeHierarchy_SupertypesResult>{}));

  handler.add<::lsp::requests::TypeHierarchy_Subtypes>(
      create_item_request_handler<::lsp::TypeHierarchy_SubtypesResult,
                                  ::lsp::TypeHierarchySubtypesParams>(
          server, sharedServices, typeHierarchyRequirement,
          [&sharedServices](const ::lsp::TypeHierarchySubtypesParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getTypeHierarchySubtypes(sharedServices, params,
                                            cancelToken);
          },
          wrap_empty_vector_as_null<::lsp::TypeHierarchy_SubtypesResult>{}));

  handler.add<::lsp::requests::TextDocument_FoldingRange>(
      create_request_handler<::lsp::TextDocument_FoldingRangeResult,
                             ::lsp::FoldingRangeParams>(
          server, sharedServices, foldingRangeRequirement,
          [&sharedServices](const ::lsp::FoldingRangeParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getFoldingRanges(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_FoldingRangeResult>{}));

  handler.add<::lsp::requests::TextDocument_Declaration>(
      create_request_handler<::lsp::TextDocument_DeclarationResult,
                             ::lsp::DeclarationParams>(
          server, sharedServices, declarationRequirement,
          [&sharedServices](const ::lsp::DeclarationParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getDeclaration(sharedServices, params, cancelToken);
          },
          wrap_optional_links<::lsp::TextDocument_DeclarationResult,
                              ::lsp::Declaration>{
              GotoLinkKind::Declaration}));

  handler.add<::lsp::requests::TextDocument_Definition>(
      create_request_handler<::lsp::TextDocument_DefinitionResult,
                             ::lsp::DefinitionParams>(
          server, sharedServices, definitionRequirement,
          [&sharedServices](const ::lsp::DefinitionParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getDefinition(sharedServices, params, cancelToken);
          },
          wrap_optional_links<::lsp::TextDocument_DefinitionResult,
                              ::lsp::Definition>{
              GotoLinkKind::Definition}));

  handler.add<::lsp::requests::TextDocument_TypeDefinition>(
      create_request_handler<::lsp::TextDocument_TypeDefinitionResult,
                             ::lsp::TypeDefinitionParams>(
          server, sharedServices, typeDefinitionRequirement,
          [&sharedServices](const ::lsp::TypeDefinitionParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getTypeDefinition(sharedServices, params, cancelToken);
          },
          wrap_optional_links<::lsp::TextDocument_TypeDefinitionResult,
                              ::lsp::Definition>{
              GotoLinkKind::TypeDefinition}));

  handler.add<::lsp::requests::TextDocument_Implementation>(
      create_request_handler<::lsp::TextDocument_ImplementationResult,
                             ::lsp::ImplementationParams>(
          server, sharedServices, implementationRequirement,
          [&sharedServices](const ::lsp::ImplementationParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getImplementation(sharedServices, params, cancelToken);
          },
          wrap_optional_links<::lsp::TextDocument_ImplementationResult,
                              ::lsp::Definition>{
              GotoLinkKind::Implementation}));

  handler.add<::lsp::requests::TextDocument_References>(
      create_request_handler<::lsp::TextDocument_ReferencesResult,
                             ::lsp::ReferenceParams>(
          server, sharedServices, referencesRequirement,
          [&sharedServices](const ::lsp::ReferenceParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getReferences(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_ReferencesResult>{}));

  handler.add<::lsp::requests::TextDocument_DocumentHighlight>(
      create_request_handler<::lsp::TextDocument_DocumentHighlightResult,
                             ::lsp::DocumentHighlightParams>(
          server, sharedServices, documentHighlightRequirement,
          [&sharedServices](const ::lsp::DocumentHighlightParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getDocumentHighlights(sharedServices, params, cancelToken);
          },
          wrap_vector_payload<::lsp::TextDocument_DocumentHighlightResult>{}));

  handler.add<::lsp::requests::TextDocument_PrepareRename>(
      create_request_handler<::lsp::TextDocument_PrepareRenameResult,
                             ::lsp::PrepareRenameParams>(
          server, sharedServices, renameRequirement,
          [&sharedServices](const ::lsp::PrepareRenameParams &params,
                            const utils::CancellationToken &cancelToken) {
            return prepareRename(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_PrepareRenameResult>{}));

  handler.add<::lsp::requests::TextDocument_Rename>(
      create_request_handler<::lsp::TextDocument_RenameResult,
                             ::lsp::RenameParams>(
          server, sharedServices, renameRequirement,
          [&sharedServices](const ::lsp::RenameParams &params,
                            const utils::CancellationToken &cancelToken) {
            return rename(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_RenameResult>{}));

  handler.add<::lsp::requests::TextDocument_CodeAction>(
      create_request_handler<::lsp::TextDocument_CodeActionResult,
                             ::lsp::CodeActionParams>(
          server, sharedServices, codeActionRequirement,
          [&sharedServices](const ::lsp::CodeActionParams &params,
                            const utils::CancellationToken &cancelToken) {
            return getCodeActions(sharedServices, params, cancelToken);
          },
          wrap_optional_payload<::lsp::TextDocument_CodeActionResult>{}));
}

} // namespace pegium::lsp
