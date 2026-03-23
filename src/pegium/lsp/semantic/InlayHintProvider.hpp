#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides inlay hints for one document range.
class InlayHintProvider {
public:
  virtual ~InlayHintProvider() noexcept = default;
  /// Returns inlay hints for `params`.
  virtual std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &document,
                const ::lsp::InlayHintParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const = 0;
};

} // namespace pegium
