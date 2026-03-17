#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class Formatter {
public:
  virtual ~Formatter() noexcept = default;
  virtual std::vector<::lsp::TextEdit>
  formatDocument(const workspace::Document &document,
                 const ::lsp::DocumentFormattingParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;

  virtual std::vector<::lsp::TextEdit>
  formatDocumentRange(const workspace::Document &document,
                      const ::lsp::DocumentRangeFormattingParams &params,
                      const utils::CancellationToken &cancelToken =
                          utils::default_cancel_token) const = 0;

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

  [[nodiscard]] virtual std::optional<::lsp::DocumentOnTypeFormattingOptions>
  formatOnTypeOptions() const noexcept {
    return std::nullopt;
  }
};

} // namespace pegium::services
