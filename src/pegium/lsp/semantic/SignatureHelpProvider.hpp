#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides signature help for one document position.
class SignatureHelpProvider {
public:
  virtual ~SignatureHelpProvider() noexcept = default;

  /// Returns the signature-help capability to advertise.
  [[nodiscard]] virtual ::lsp::SignatureHelpOptions
  signatureHelpOptions() const {
    return {};
  }

  /// Returns signature help for `params`.
  virtual std::optional<::lsp::SignatureHelp>
  provideSignatureHelp(const workspace::Document &document,
                       const ::lsp::SignatureHelpParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token) const = 0;
};

} // namespace pegium
