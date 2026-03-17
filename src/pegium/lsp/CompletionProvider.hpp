#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class CompletionProvider {
public:
  virtual ~CompletionProvider() noexcept = default;

  [[nodiscard]] virtual std::optional<::lsp::CompletionOptions>
  completionOptions() const noexcept {
    return std::nullopt;
  }

  virtual std::optional<::lsp::CompletionList>
  getCompletion(const workspace::Document &document,
                const ::lsp::CompletionParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
