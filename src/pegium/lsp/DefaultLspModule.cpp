#include <pegium/lsp/DefaultLspModule.hpp>

#include <memory>

#include <pegium/lsp/DefaultDocumentUpdateHandler.hpp>
#include <pegium/lsp/DefaultFuzzyMatcher.hpp>
#include <pegium/lsp/DefaultLanguageServer.hpp>
#include <pegium/lsp/DefaultNodeKindProvider.hpp>
#include <pegium/lsp/DefaultCodeActionProvider.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

void installDefaultLspServices(services::Services &services) {
  if (!services.lsp.completionProvider) {
    services.lsp.completionProvider =
        std::make_unique<DefaultCompletionProvider>(services);
  }
  if (!services.lsp.hoverProvider) {
    services.lsp.hoverProvider = std::make_unique<MultilineCommentHoverProvider>(services);
  }
  if (!services.lsp.documentSymbolProvider) {
    services.lsp.documentSymbolProvider =
        std::make_unique<DefaultDocumentSymbolProvider>(services);
  }
  if (!services.lsp.documentHighlightProvider) {
    services.lsp.documentHighlightProvider =
        std::make_unique<DefaultDocumentHighlightProvider>(services);
  }
  if (!services.lsp.foldingRangeProvider) {
    services.lsp.foldingRangeProvider =
        std::make_unique<DefaultFoldingRangeProvider>(services);
  }
  if (!services.lsp.definitionProvider) {
    services.lsp.definitionProvider =
        std::make_unique<DefaultDefinitionProvider>(services);
  }
  if (!services.lsp.referencesProvider) {
    services.lsp.referencesProvider =
        std::make_unique<DefaultReferencesProvider>(services);
  }
  if (!services.lsp.renameProvider) {
    services.lsp.renameProvider = std::make_unique<DefaultRenameProvider>(services);
  }
  if (!services.lsp.codeActionProvider) {
    services.lsp.codeActionProvider =
        std::make_unique<services::DefaultCodeActionProvider>();
  }
}

void installDefaultSharedLspServices(
    services::SharedServices &sharedServices) {
  if (!sharedServices.lsp.documentUpdateHandler) {
    sharedServices.lsp.documentUpdateHandler =
        std::make_unique<DefaultDocumentUpdateHandler>(sharedServices);
  }
  if (!sharedServices.lsp.fuzzyMatcher) {
    sharedServices.lsp.fuzzyMatcher =
        std::make_unique<DefaultFuzzyMatcher>();
  }
  if (!sharedServices.lsp.languageServer) {
    sharedServices.lsp.languageServer =
        std::make_unique<DefaultLanguageServer>(sharedServices);
  }
  if (!sharedServices.lsp.nodeKindProvider) {
    sharedServices.lsp.nodeKindProvider =
        std::make_unique<DefaultNodeKindProvider>(sharedServices);
  }
  if (!sharedServices.lsp.workspaceSymbolProvider) {
    sharedServices.lsp.workspaceSymbolProvider =
        std::make_unique<DefaultWorkspaceSymbolProvider>(sharedServices);
  }
}

} // namespace pegium::lsp
