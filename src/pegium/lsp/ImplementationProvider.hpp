#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/workspace/Document.hpp>

namespace pegium::services {

class ImplementationProvider {
public:
  virtual ~ImplementationProvider() noexcept = default;
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &document,
                    const ::lsp::ImplementationParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token) const = 0;
};

} // namespace pegium::services
