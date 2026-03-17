#include <pegium/lsp/MultilineCommentHoverProvider.hpp>

#include <pegium/documentation/DocumentationProvider.hpp>
#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

std::optional<::lsp::Hover> MultilineCommentHoverProvider::getHoverContent(
    const workspace::Document &document, const ::lsp::HoverParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.positionToOffset(params.position);

  const auto *references = languageServices.references.references.get();
  const auto *documentationProvider =
      languageServices.documentation.documentationProvider.get();
  if (references == nullptr || documentationProvider == nullptr) {
    return std::nullopt;
  }

  const auto declaration = references->findDeclarationAt(document, offset);
  if (!declaration.has_value() || declaration->node == nullptr) {
    return std::nullopt;
  }

  const auto documentation =
      documentationProvider->getDocumentation(*declaration->node);
  if (!documentation.has_value() || documentation->empty()) {
    return std::nullopt;
  }

  ::lsp::MarkupContent markup{};
  markup.kind = ::lsp::MarkupKind::Markdown;
  markup.value = *documentation;

  ::lsp::Hover hover{};
  hover.contents = std::move(markup);
  return hover;
}

} // namespace pegium::lsp
