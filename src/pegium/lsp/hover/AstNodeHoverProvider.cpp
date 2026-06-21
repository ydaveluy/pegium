#include <pegium/lsp/hover/AstNodeHoverProvider.hpp>

#include <utility>

#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>

namespace pegium {

std::optional<::lsp::Hover> AstNodeHoverProvider::getHoverContent(
    const workspace::Document &document, const ::lsp::HoverParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.textDocument().offsetAt(params.position);

  // Keyword branch: a position over a keyword renders the keyword's own hover
  // (by default its `.doc("…")` documentation). Checked first so a keyword
  // token never falls through to cross-reference resolution.
  if (document.parseResult.cst != nullptr) {
    if (const auto node =
            find_node_at_offset(*document.parseResult.cst, offset)) {
      if (const auto *grammarElement = node->getGrammarElement()) {
        if (auto keywordHover = getKeywordHoverContent(*grammarElement)) {
          return keywordHover;
        }
      }
    }
  }

  // AST-node branch: resolve the declaration(s) at the position and defer to
  // the concrete renderer.
  const auto &references = *services.references.references;
  const auto declarations = provider_detail::find_declarations_at_offset(
      document, offset, references);
  if (declarations.empty()) {
    return std::nullopt;
  }
  return getAstNodeHoverContent(declarations);
}

std::optional<::lsp::Hover> AstNodeHoverProvider::getKeywordHoverContent(
    const grammar::AbstractElement &keyword) const {
  const auto &documentationProvider =
      *services.documentation.documentationProvider;
  auto documentation = documentationProvider.getDocumentation(keyword);
  if (!documentation.has_value() || documentation->empty()) {
    return std::nullopt;
  }

  ::lsp::MarkupContent markup{};
  markup.kind = ::lsp::MarkupKind::Markdown;
  markup.value = std::move(*documentation);

  ::lsp::Hover hover{};
  hover.contents = std::move(markup);
  return hover;
}

} // namespace pegium
