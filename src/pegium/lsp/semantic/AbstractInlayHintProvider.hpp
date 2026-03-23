#pragma once

#include <functional>

#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {

/// Callback used by inlay-hint providers to emit one hint at a time.
using InlayHintAcceptor = std::function<void(::lsp::InlayHint hint)>;

/// Shared inlay-hint provider that walks the AST and delegates hint emission.
class AbstractInlayHintProvider : protected DefaultLanguageService,
                                public ::pegium::InlayHintProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &document,
                const ::lsp::InlayHintParams &params,
                const utils::CancellationToken &cancelToken) const override;

protected:
  /// Emits inlay hints associated with one AST node.
  virtual void computeInlayHint(const AstNode &astNode,
                                const InlayHintAcceptor &acceptor) const = 0;
};

} // namespace pegium
