#include <pegium/lsp/symbols/DefaultWorkspaceSymbolProvider.hpp>

#include <cassert>
#include <cctype>
#include <memory>
#include <string>
#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

namespace {

::lsp::Location symbol_location(const pegium::SharedServices &sharedServices,
                                const workspace::AstNodeDescription &entry) {
  assert(entry.nameLength > 0);
  const auto document =
      sharedServices.workspace.documents->getDocument(entry.documentId);
  assert(document != nullptr);

  ::lsp::Location location{};
  location.uri = ::lsp::Uri::parse(document->uri);
  location.range.start = document->textDocument().positionAt(entry.offset);
  location.range.end =
      document->textDocument().positionAt(entry.offset + entry.nameLength);
  return location;
}

} // namespace

std::vector<::lsp::WorkspaceSymbol>
DefaultWorkspaceSymbolProvider::getSymbols(
    const ::lsp::WorkspaceSymbolParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &indexManager = *shared.workspace.indexManager;
  const auto &nodeKindProvider = *shared.lsp.nodeKindProvider;
  const auto &fuzzyMatcher = *shared.lsp.fuzzyMatcher;

  std::string query = params.query;
  std::ranges::transform(query, query.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });

  std::vector<::lsp::WorkspaceSymbol> symbols;
  auto allElements = indexManager.allElements();
  for (const auto &entry : allElements) {
    utils::throw_if_cancelled(cancelToken);
    if (!fuzzyMatcher.match(query, entry.name)) {
      continue;
    }

    ::lsp::WorkspaceSymbol workspaceSymbol{};
    workspaceSymbol.name = entry.name;
    workspaceSymbol.kind = nodeKindProvider.getSymbolKind(entry);
    workspaceSymbol.location = symbol_location(shared, entry);
    symbols.push_back(std::move(workspaceSymbol));
  }
  return symbols;
}

} // namespace pegium
