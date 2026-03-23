#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Shared call-hierarchy provider that resolves one node then delegates edge collection.
class AbstractCallHierarchyProvider : protected DefaultLanguageService,
                                public ::pegium::CallHierarchyProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

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
  /// Returns incoming call edges targeting `node`.
  [[nodiscard]] virtual std::vector<::lsp::CallHierarchyIncomingCall>
  getIncomingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const = 0;

  /// Returns outgoing call edges originating from `node`.
  [[nodiscard]] virtual std::vector<::lsp::CallHierarchyOutgoingCall>
  getOutgoingCalls(const AstNode &node,
                   const utils::CancellationToken &cancelToken) const = 0;

  /// Customizes the item built for `node` before it is returned.
  virtual void customizeCallHierarchyItem(const AstNode &,
                                          ::lsp::CallHierarchyItem &) const {}

private:
  [[nodiscard]] std::optional<::lsp::CallHierarchyItem>
  createCallHierarchyItem(const AstNode &node) const;
};

} // namespace pegium
