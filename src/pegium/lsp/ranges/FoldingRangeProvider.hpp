#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides folding ranges for one document.
class FoldingRangeProvider {
public:
  virtual ~FoldingRangeProvider() noexcept = default;
  /// Returns the folding ranges relevant to `document`.
  virtual std::vector<::lsp::FoldingRange>
  getFoldingRanges(const workspace::Document &document,
                   const ::lsp::FoldingRangeParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token) const = 0;
};

} // namespace pegium
