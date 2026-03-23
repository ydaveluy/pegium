#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides hover content for one document position.
class HoverProvider {
public:
  virtual ~HoverProvider() noexcept = default;
  /// Returns hover content for `params`, or `std::nullopt` when unavailable.
  virtual std::optional<::lsp::Hover>
  getHoverContent(const workspace::Document &document,
                  const ::lsp::HoverParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const = 0;
};

} // namespace pegium
