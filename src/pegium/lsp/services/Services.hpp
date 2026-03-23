#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pegium/lsp/hierarchy/CallHierarchyProvider.hpp>
#include <pegium/lsp/code-actions/CodeActionProvider.hpp>
#include <pegium/lsp/code-actions/CodeLensProvider.hpp>
#include <pegium/lsp/completion/CompletionProvider.hpp>
#include <pegium/lsp/navigation/DeclarationProvider.hpp>
#include <pegium/lsp/navigation/DefinitionProvider.hpp>
#include <pegium/lsp/formatting/Formatter.hpp>
#include <pegium/lsp/navigation/DocumentHighlightProvider.hpp>
#include <pegium/lsp/navigation/DocumentLinkProvider.hpp>
#include <pegium/lsp/symbols/DocumentSymbolProvider.hpp>
#include <pegium/lsp/ranges/FoldingRangeProvider.hpp>
#include <pegium/lsp/hover/HoverProvider.hpp>
#include <pegium/lsp/navigation/ImplementationProvider.hpp>
#include <pegium/lsp/semantic/InlayHintProvider.hpp>
#include <pegium/lsp/navigation/ReferencesProvider.hpp>
#include <pegium/lsp/navigation/RenameProvider.hpp>
#include <pegium/lsp/semantic/SemanticTokenProvider.hpp>
#include <pegium/lsp/ranges/SelectionRangeProvider.hpp>
#include <pegium/lsp/semantic/SignatureHelpProvider.hpp>
#include <pegium/lsp/navigation/TypeDefinitionProvider.hpp>
#include <pegium/lsp/hierarchy/TypeHierarchyProvider.hpp>
#include <pegium/lsp/symbols/WorkspaceSymbolProvider.hpp>
#include <pegium/core/services/CoreServices.hpp>

namespace pegium {

struct SharedServices;

/// LSP feature services owned by one language.
struct LspFeatureServices {
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<CompletionProvider> completionProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<HoverProvider> hoverProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<DocumentSymbolProvider> documentSymbolProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<DocumentHighlightProvider> documentHighlightProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<FoldingRangeProvider> foldingRangeProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<DeclarationProvider> declarationProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<DefinitionProvider> definitionProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<TypeDefinitionProvider> typeProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<ImplementationProvider> implementationProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<ReferencesProvider> referencesProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<RenameProvider> renameProvider;
  // LSP feature service; installed by default, but overridable.
  std::unique_ptr<CodeActionProvider> codeActionProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<DocumentLinkProvider> documentLinkProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<SelectionRangeProvider> selectionRangeProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<SignatureHelpProvider> signatureHelp;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<CodeLensProvider> codeLensProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<Formatter> formatter;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<InlayHintProvider> inlayHintProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<SemanticTokenProvider> semanticTokenProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<CallHierarchyProvider> callHierarchyProvider;
  // Optional LSP feature; capability is advertised only when provided.
  std::unique_ptr<TypeHierarchyProvider> typeHierarchyProvider;
};

/// Language service container that extends core services with LSP features.
struct Services : services::CoreServices {
  using MetaData = services::LanguageMetaData;

  explicit Services(const SharedServices &sharedServices);
  Services(Services &&) noexcept = default;
  Services &operator=(Services &&) noexcept = delete;
  Services(const Services &) = delete;
  Services &operator=(const Services &) = delete;
  ~Services() noexcept override;

  const SharedServices &shared;
  LspFeatureServices lsp;
};

} // namespace pegium
