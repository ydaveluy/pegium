#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class AbstractCallHierarchyProvider : protected services::DefaultLanguageService,
                                public services::CallHierarchyProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  std::vector<::lsp::CallHierarchyItem>
  prepareCallHierarchy(const workspace::Document &document,
                       const ::lsp::CallHierarchyPrepareParams &params,
                       const utils::CancellationToken &cancelToken) const override;

  std::vector<::lsp::CallHierarchyIncomingCall>
  incomingCalls(const ::lsp::CallHierarchyIncomingCallsParams &params,
                const utils::CancellationToken &cancelToken) const override;

  std::vector<::lsp::CallHierarchyOutgoingCall>
  outgoingCalls(const ::lsp::CallHierarchyOutgoingCallsParams &params,
                const utils::CancellationToken &cancelToken) const override;

protected:
  [[nodiscard]] virtual std::vector<::lsp::CallHierarchyIncomingCall>
  getIncomingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const = 0;

  [[nodiscard]] virtual std::vector<::lsp::CallHierarchyOutgoingCall>
  getOutgoingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const = 0;

  virtual void customizeCallHierarchyItem(const AstNode &,
                                          ::lsp::CallHierarchyItem &) const {}

private:
  [[nodiscard]] std::optional<::lsp::CallHierarchyItem>
  createCallHierarchyItem(const AstNode &node) const;
};

} // namespace pegium::lsp
