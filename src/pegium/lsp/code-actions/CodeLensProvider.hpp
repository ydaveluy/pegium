#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides code lenses for one document.
class CodeLensProvider {
public:
  virtual ~CodeLensProvider() noexcept = default;
  /// Returns the code lenses to show for `document`.
  virtual std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &document,
                  const ::lsp::CodeLensParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const = 0;

  /// Returns whether this provider supports deferred code-lens resolution.
  [[nodiscard]] virtual bool supportsResolveCodeLens() const noexcept {
    return false;
  }

  /// Resolves extra metadata for one code lens returned by `provideCodeLens(...)`.
  ///
  /// Pegium restores the original `CodeLens::data` before calling this hook and
  /// preserves it across repeated resolve requests.
  virtual std::optional<::lsp::CodeLens>
  resolveCodeLens(const ::lsp::CodeLens &codeLens,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const {
    (void)codeLens;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium
