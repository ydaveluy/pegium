#pragma once

#include <optional>
#include <vector>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/hover/HoverProvider.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Base hover provider that dispatches a position to either a keyword or an AST
/// declaration, mirroring Langium's `AstNodeHoverProvider`:
///  - over a keyword CST node, it renders `getKeywordHoverContent` (by default
///    the documentation attached via `"kw"_kw.doc("…")`);
///  - otherwise it resolves the declaration(s) at the position and defers their
///    rendering to the concrete `getAstNodeHoverContent`.
class AstNodeHoverProvider : protected DefaultLanguageService,
                             public ::pegium::HoverProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::optional<::lsp::Hover>
  getHoverContent(const workspace::Document &document,
                  const ::lsp::HoverParams &params,
                  const utils::CancellationToken &cancelToken) const override;

protected:
  /// Hover content for a keyword CST node, given its grammar element. The
  /// default renders the documentation attached via `"kw"_kw.doc("…")` through
  /// the DocumentationProvider; override to customise keyword hovers.
  [[nodiscard]] virtual std::optional<::lsp::Hover>
  getKeywordHoverContent(const grammar::AbstractElement &keyword) const;

  /// Hover content for the AST declaration(s) resolved at the position.
  /// Implemented by concrete providers (e.g. comment rendering).
  [[nodiscard]] virtual std::optional<::lsp::Hover>
  getAstNodeHoverContent(
      const std::vector<const AstNode *> &declarations) const = 0;
};

} // namespace pegium
