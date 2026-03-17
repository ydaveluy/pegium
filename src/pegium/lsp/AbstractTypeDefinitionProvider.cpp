#include <pegium/lsp/AbstractTypeDefinitionProvider.hpp>

namespace pegium::lsp {

std::optional<std::vector<::lsp::LocationLink>>
AbstractTypeDefinitionProvider::getTypeDefinition(
    const workspace::Document &document,
    const ::lsp::TypeDefinitionParams &params,
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
  return collectGoToTypeLocationLinks(*declaration->node, cancelToken);
}

} // namespace pegium::lsp
