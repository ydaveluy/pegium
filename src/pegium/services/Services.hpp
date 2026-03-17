#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <pegium/lsp/CallHierarchyProvider.hpp>
#include <pegium/lsp/CodeActionProvider.hpp>
#include <pegium/lsp/CodeLensProvider.hpp>
#include <pegium/lsp/CompletionProvider.hpp>
#include <pegium/lsp/DeclarationProvider.hpp>
#include <pegium/lsp/DefinitionProvider.hpp>
#include <pegium/lsp/Formatter.hpp>
#include <pegium/lsp/DocumentHighlightProvider.hpp>
#include <pegium/lsp/DocumentLinkProvider.hpp>
#include <pegium/lsp/DocumentSymbolProvider.hpp>
#include <pegium/lsp/FoldingRangeProvider.hpp>
#include <pegium/lsp/HoverProvider.hpp>
#include <pegium/lsp/ImplementationProvider.hpp>
#include <pegium/lsp/InlayHintProvider.hpp>
#include <pegium/lsp/ReferencesProvider.hpp>
#include <pegium/lsp/RenameProvider.hpp>
#include <pegium/lsp/SemanticTokenProvider.hpp>
#include <pegium/lsp/SelectionRangeProvider.hpp>
#include <pegium/lsp/SignatureHelpProvider.hpp>
#include <pegium/lsp/TypeDefinitionProvider.hpp>
#include <pegium/lsp/TypeHierarchyProvider.hpp>
#include <pegium/lsp/WorkspaceSymbolProvider.hpp>
#include <pegium/services/CoreServices.hpp>

namespace pegium::services {

struct SharedServices;

struct LspFeatureServices {
  std::unique_ptr<CompletionProvider> completionProvider;
  std::unique_ptr<HoverProvider> hoverProvider;
  std::unique_ptr<DocumentSymbolProvider> documentSymbolProvider;
  std::unique_ptr<DocumentHighlightProvider> documentHighlightProvider;
  std::unique_ptr<FoldingRangeProvider> foldingRangeProvider;
  std::unique_ptr<DeclarationProvider> declarationProvider;
  std::unique_ptr<DefinitionProvider> definitionProvider;
  std::unique_ptr<TypeDefinitionProvider> typeProvider;
  std::unique_ptr<ImplementationProvider> implementationProvider;
  std::unique_ptr<ReferencesProvider> referencesProvider;
  std::unique_ptr<RenameProvider> renameProvider;
  std::unique_ptr<CodeActionProvider> codeActionProvider;
  std::unique_ptr<DocumentLinkProvider> documentLinkProvider;
  std::unique_ptr<SelectionRangeProvider> selectionRangeProvider;
  std::unique_ptr<SignatureHelpProvider> signatureHelp;
  std::unique_ptr<CodeLensProvider> codeLensProvider;
  std::unique_ptr<Formatter> formatter;
  std::unique_ptr<InlayHintProvider> inlayHintProvider;
  std::unique_ptr<SemanticTokenProvider> semanticTokenProvider;
  std::unique_ptr<CallHierarchyProvider> callHierarchyProvider;
  std::unique_ptr<TypeHierarchyProvider> typeHierarchyProvider;
};

struct LspServices {
  LspFeatureServices lsp;
};

struct Services : CoreServices, LspServices {
  using MetaData = LanguageMetaData;

  explicit Services(const SharedServices &sharedServices);
  ~Services() noexcept override;

  const SharedServices &sharedServices;
};

std::unique_ptr<Services>
makeDefaultServices(const SharedServices &sharedServices,
                    std::string languageId);

} // namespace pegium::services
