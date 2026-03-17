#pragma once

#include <pegium/lsp/DefaultCompletionProvider.hpp>
#include <pegium/lsp/DefaultDefinitionProvider.hpp>
#include <pegium/lsp/DefaultDocumentHighlightProvider.hpp>
#include <pegium/lsp/DefaultDocumentSymbolProvider.hpp>
#include <pegium/lsp/DefaultFoldingRangeProvider.hpp>
#include <pegium/lsp/MultilineCommentHoverProvider.hpp>
#include <pegium/lsp/DefaultReferencesProvider.hpp>
#include <pegium/lsp/DefaultRenameProvider.hpp>
#include <pegium/lsp/DefaultWorkspaceSymbolProvider.hpp>

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {

void installDefaultLspServices(services::Services &services);
void installDefaultSharedLspServices(services::SharedServices &sharedServices);

} // namespace pegium::lsp
