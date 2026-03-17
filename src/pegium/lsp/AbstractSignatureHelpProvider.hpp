#pragma once

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/services/DefaultLanguageService.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {

class AbstractSignatureHelpProvider : protected services::DefaultLanguageService,
                                public services::SignatureHelpProvider {
public:
  using services::DefaultLanguageService::DefaultLanguageService;

  [[nodiscard]] ::lsp::SignatureHelpOptions
  signatureHelpOptions() const override;

  std::optional<::lsp::SignatureHelp>
  provideSignatureHelp(const workspace::Document &document,
                       const ::lsp::SignatureHelpParams &params,
                       const utils::CancellationToken &cancelToken) const override;

protected:
  [[nodiscard]] virtual std::optional<::lsp::SignatureHelp>
  getSignatureFromElement(
      const AstNode &element,
      const utils::CancellationToken &cancelToken) const = 0;
};

} // namespace pegium::lsp
