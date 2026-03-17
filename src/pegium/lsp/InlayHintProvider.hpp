#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class InlayHintProvider {
public:
  virtual ~InlayHintProvider() noexcept = default;
  virtual std::vector<::lsp::InlayHint>
  getInlayHints(const workspace::Document &document,
                const ::lsp::InlayHintParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
