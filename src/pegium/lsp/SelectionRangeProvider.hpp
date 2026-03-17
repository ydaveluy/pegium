#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class SelectionRangeProvider {
public:
  virtual ~SelectionRangeProvider() noexcept = default;
  virtual std::vector<::lsp::SelectionRange>
  getSelectionRanges(const workspace::Document &document,
                     const ::lsp::SelectionRangeParams &params,
                     const utils::CancellationToken &cancelToken =
                         utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
