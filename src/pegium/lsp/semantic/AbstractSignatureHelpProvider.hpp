#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/lsp/services/DefaultLanguageService.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {

/// Shared signature-help provider that resolves one AST element before delegating.
class AbstractSignatureHelpProvider : protected DefaultLanguageService,
                                public ::pegium::SignatureHelpProvider {
public:
  using DefaultLanguageService::DefaultLanguageService;

  [[nodiscard]] ::lsp::SignatureHelpOptions
  signatureHelpOptions() const override;

  std::optional<::lsp::SignatureHelp>
  provideSignatureHelp(const workspace::Document &document,
                       const ::lsp::SignatureHelpParams &params,
                       const utils::CancellationToken &cancelToken) const override;

protected:
  /// Returns signature help for the resolved AST element.
  [[nodiscard]] virtual std::optional<::lsp::SignatureHelp>
  getSignatureFromElement(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium
