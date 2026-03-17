#pragma once

#include <functional>

#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/syntax-tree/AstNode.hpp>

namespace pegium::lsp {

using InlayHintAcceptor = std::function<void(::lsp::InlayHint hint)>;

class AbstractInlayHintProvider : protected services::DefaultLanguageService,
                                public services::InlayHintProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &document,
                const ::lsp::InlayHintParams &params,
                const utils::CancellationToken &cancelToken) const override;

protected:
  virtual void computeInlayHint(const AstNode &astNode,
                                const InlayHintAcceptor &acceptor) const = 0;
};

} // namespace pegium::lsp
