#include <pegium/lsp/semantic/AbstractInlayHintProvider.hpp>

namespace pegium {

std::vector<::lsp::InlayHint>
AbstractInlayHintProvider::getInlayHints(const workspace::Document &document,
                                        const ::lsp::InlayHintParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  if (!document.hasAst()) {
    return {};
  }

  const auto &textDocument = document.textDocument();
  const auto rangeBegin = textDocument.offsetAt(params.range.start);
  const auto rangeEnd = textDocument.offsetAt(params.range.end);

  std::vector<::lsp::InlayHint> hints;
  const InlayHintAcceptor acceptor = [&hints](::lsp::InlayHint hint) {
    hints.push_back(std::move(hint));
  };

  auto visit = [this, rangeBegin, rangeEnd, &cancelToken,
                &acceptor](const auto &self, const AstNode &node) {
    if (!node.hasCstNode()) {
      return;
    }
    if (const auto cstNode = node.getCstNode();
        cstNode.getEnd() < rangeBegin || cstNode.getBegin() > rangeEnd) {
      return;
    }

    utils::throw_if_cancelled(cancelToken);
    computeInlayHint(node, acceptor);
    for (const auto *child : node.getContent()) {
      self(self, *child);
    }
  };

  visit(visit, *document.parseResult.value);
  return hints;
}

} // namespace pegium
