#include <pegium/lsp/hover/MultilineCommentHoverProvider.hpp>

#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <cassert>

namespace pegium {

std::optional<::lsp::Hover> MultilineCommentHoverProvider::getHoverContent(
    const workspace::Document &document, const ::lsp::HoverParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.textDocument().offsetAt(params.position);

  const auto &references = *services.references.references;
  const auto &documentationProvider =
      *services.documentation.documentationProvider;
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, offset, references);
  if (declarations.empty()) {
    return std::nullopt;
  }

  const auto documentation =
      documentationProvider.getDocumentation(*declarations.front());
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

} // namespace pegium
