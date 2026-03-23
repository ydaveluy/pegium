#pragma once

#include <optional>
#include <string>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Server capability options advertised by one completion provider.
struct CompletionProviderOptions {
  std::vector<std::string> triggerCharacters;
  std::vector<std::string> allCommitCharacters;
};

/// Provides completion items for one document and cursor position.
class CompletionProvider {
public:
  virtual ~CompletionProvider() noexcept = default;

  /// Returns capability options to advertise for this provider.
  [[nodiscard]] virtual std::optional<CompletionProviderOptions>
  completionOptions() const noexcept {
    return std::nullopt;
  }

  /// Returns completions for `params`, or `std::nullopt` when none are available.
  virtual std::optional<::lsp::CompletionList>
  getCompletion(const workspace::Document &document,
                const ::lsp::CompletionParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const = 0;
};

} // namespace pegium
