#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Formats documents, ranges, or typed characters into LSP text edits.
class Formatter {
public:
  virtual ~Formatter() noexcept = default;
  /// Formats the full document.
  virtual std::vector<::lsp::TextEdit>
  formatDocument(const workspace::Document &document,
                 const ::lsp::DocumentFormattingParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;

  /// Formats a selected document range.
  virtual std::vector<::lsp::TextEdit>
  formatDocumentRange(const workspace::Document &document,
                      const ::lsp::DocumentRangeFormattingParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token) const = 0;

  /// Formats in reaction to a typed character.
  virtual std::vector<::lsp::TextEdit>
  formatDocumentOnType(const workspace::Document &document,
                       const ::lsp::DocumentOnTypeFormattingParams &params,
                       const utils::CancellationToken &cancelToken =
                           utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return {};
  }

  /// Returns the on-type formatting capability to advertise, when any.
  [[nodiscard]] virtual std::optional<::lsp::DocumentOnTypeFormattingOptions>
  formatOnTypeOptions() const noexcept {
    return std::nullopt;
  }
};

} // namespace pegium
