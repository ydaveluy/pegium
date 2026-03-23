#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides nested selection ranges for one or more cursor positions.
class SelectionRangeProvider {
public:
  virtual ~SelectionRangeProvider() noexcept = default;
  /// Returns selection ranges for the positions in `params`.
  virtual std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &document,
                     const ::lsp::SelectionRangeParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token) const = 0;
};

} // namespace pegium
