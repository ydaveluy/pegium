#include <pegium/lsp/AbstractGoToImplementationProvider.hpp>

namespace pegium::lsp {

std::optional<std::vector<::lsp::LocationLink>>
AbstractGoToImplementationProvider::getImplementation(
    const workspace::Document &document,
    const ::lsp::ImplementationParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto *references = languageServices.references.references.get();
  if (references == nullptr) {
    return std::nullopt;
  }
  const auto declaration =
      references->findDeclarationAt(document, document.positionToOffset(params.position));
  if (!declaration.has_value() || declaration->node == nullptr) {
    return std::nullopt;
  }
  return collectGoToImplementationLocationLinks(*declaration->node,
                                                cancelToken);
}

} // namespace pegium::lsp
