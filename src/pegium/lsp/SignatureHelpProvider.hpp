#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class SignatureHelpProvider {
public:
  virtual ~SignatureHelpProvider() noexcept = default;

  [[nodiscard]] virtual ::lsp::SignatureHelpOptions
  signatureHelpOptions() const {
    return {};
  }

  virtual std::optional<::lsp::SignatureHelp>
  provideSignatureHelp(const workspace::Document &document,
                       const ::lsp::SignatureHelpParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
