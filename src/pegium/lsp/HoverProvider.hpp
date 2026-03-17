#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class HoverProvider {
public:
  virtual ~HoverProvider() noexcept = default;
  virtual std::optional<::lsp::Hover>
  getHoverContent(const workspace::Document &document,
                  const ::lsp::HoverParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
