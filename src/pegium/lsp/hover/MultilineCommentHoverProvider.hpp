#pragma once

#include <optional>
#include <vector>

#include <pegium/lsp/hover/AstNodeHoverProvider.hpp>

namespace pegium {

/// Hover provider that renders the documentation of the declaration(s) at the
/// position from their preceding multiline comments. Keyword hovers
/// (`"kw"_kw.doc("…")`) are handled by the `AstNodeHoverProvider` base.
class MultilineCommentHoverProvider : public AstNodeHoverProvider {
public:
  using AstNodeHoverProvider::AstNodeHoverProvider;

protected:
  [[nodiscard]] std::optional<::lsp::Hover>
  getAstNodeHoverContent(
      const std::vector<const AstNode *> &declarations) const override;
};

} // namespace pegium
