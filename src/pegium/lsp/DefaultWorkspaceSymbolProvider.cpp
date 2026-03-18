#include <pegium/lsp/DefaultWorkspaceSymbolProvider.hpp>

#include <cctype>
#include <memory>
#include <string>
#include <utility>

#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

namespace {

std::shared_ptr<workspace::Document>
document_for_symbol(const services::SharedServices &sharedServices,
                    workspace::DocumentId documentId) {
  if (sharedServices.workspace.documents == nullptr) {
    return nullptr;
  }
  return sharedServices.workspace.documents->getDocument(documentId);
}

std::optional<::lsp::Location>
symbol_location(const services::SharedServices &sharedServices,
                const workspace::AstNodeDescription &entry) {
  if (entry.nameLength == 0) {
    return std::nullopt;
  }
  const auto document =
      document_for_symbol(sharedServices, entry.documentId);
  if (document == nullptr) {
    return std::nullopt;
  }

  ::lsp::Location location{};
  location.uri = ::lsp::Uri::parse(document->uri);
  location.range.start = document->offsetToPosition(entry.offset);
  location.range.end =
      document->offsetToPosition(entry.offset + entry.nameLength);
  return location;
}

} // namespace

std::vector<::lsp::WorkspaceSymbol>
DefaultWorkspaceSymbolProvider::getSymbols(
    const ::lsp::WorkspaceSymbolParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *indexManager = sharedServices.workspace.indexManager.get();
  const auto *nodeKindProvider = sharedServices.lsp.nodeKindProvider.get();
  const auto *fuzzyMatcher = sharedServices.lsp.fuzzyMatcher.get();
  if (indexManager == nullptr || nodeKindProvider == nullptr ||
      fuzzyMatcher == nullptr) {
    return {};
  }

  std::string query = params.query;
  std::ranges::transform(query, query.begin(), [](unsigned char value) {
    return static_cast<char>(std::tolower(value));
  });

  std::vector<::lsp::WorkspaceSymbol> symbols;
  auto allElements = indexManager->allElements();
  for (const auto &entry : allElements) {
    utils::throw_if_cancelled(cancelToken);
    if (!fuzzyMatcher->match(query, entry.name)) {
      continue;
    }

    ::lsp::WorkspaceSymbol workspaceSymbol{};
    workspaceSymbol.name = entry.name;
    workspaceSymbol.kind = nodeKindProvider->getSymbolKind(entry);
    if (const auto location = symbol_location(sharedServices, entry);
        location.has_value()) {
      workspaceSymbol.location = *location;
    } else {
      continue;
    }
    symbols.push_back(std::move(workspaceSymbol));
  }
  return symbols;
}

} // namespace pegium::lsp
