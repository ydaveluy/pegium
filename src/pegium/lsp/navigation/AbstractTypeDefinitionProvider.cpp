#include <pegium/lsp/navigation/AbstractTypeDefinitionProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {

std::optional<std::vector<::lsp::LocationLink>>
AbstractTypeDefinitionProvider::getTypeDefinition(
    const workspace::Document &document,
    const ::lsp::TypeDefinitionParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &references = *services.references.references;
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, document.textDocument().offsetAt(params.position),
      references);
  // Collect links for every resolved declaration (a position can resolve to
  // several, e.g. a multi-reference), instead of using only the first.
  std::vector<::lsp::LocationLink> links;
  for (const auto *declaration : declarations) {
    utils::throw_if_cancelled(cancelToken);
    if (auto sublinks = collectGoToTypeLocationLinks(*declaration, cancelToken);
        sublinks.has_value()) {
      for (auto &link : *sublinks) {
        links.push_back(std::move(link));
      }
    }
  }
  if (links.empty()) {
    return std::nullopt;
  }
  return links;
}

} // namespace pegium
