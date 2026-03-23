#include <pegium/lsp/navigation/AbstractGoToImplementationProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <cassert>

namespace pegium {

std::optional<std::vector<::lsp::LocationLink>>
AbstractGoToImplementationProvider::getImplementation(
    const workspace::Document &document,
    const ::lsp::ImplementationParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &references = *services.references.references;
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, document.textDocument().offsetAt(params.position),
      references);
  if (declarations.empty()) {
    return std::nullopt;
  }
  return collectGoToImplementationLocationLinks(*declarations.front(),
                                                cancelToken);
}

} // namespace pegium
