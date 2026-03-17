#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class FoldingRangeProvider {
public:
  virtual ~FoldingRangeProvider() noexcept = default;
  virtual std::vector<::lsp::FoldingRange>
  getFoldingRanges(const workspace::Document &document,
                   const ::lsp::FoldingRangeParams &params,
                   const utils::CancellationToken &cancelToken =
                       utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
