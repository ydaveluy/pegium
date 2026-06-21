#include <pegium/lsp/hover/MultilineCommentHoverProvider.hpp>

#include <string>
#include <utility>

#include <pegium/core/documentation/DocumentationProvider.hpp>

namespace pegium {

std::optional<::lsp::Hover>
MultilineCommentHoverProvider::getAstNodeHoverContent(
    const std::vector<const AstNode *> &declarations) const {
  const auto &documentationProvider =
      *services.documentation.documentationProvider;

  // Concatenate the documentation of every resolved declaration (a position can
  // resolve to several, e.g. a multi-reference), joined with a space, rather
  // than rendering only the first.
  std::string combined;
  for (const auto *declaration : declarations) {
    const auto documentation =
        documentationProvider.getDocumentation(*declaration);
    if (documentation.has_value() && !documentation->empty()) {
      if (!combined.empty()) {
        combined += ' ';
      }
      combined += *documentation;
    }
  }
  if (combined.empty()) {
    return std::nullopt;
  }

  ::lsp::MarkupContent markup{};
  markup.kind = ::lsp::MarkupKind::Markdown;
  markup.value = std::move(combined);

  ::lsp::Hover hover{};
  hover.contents = std::move(markup);
  return hover;
}

} // namespace pegium
