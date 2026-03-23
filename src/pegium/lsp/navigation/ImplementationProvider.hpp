#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides implementation targets for one document position.
class ImplementationProvider {
public:
  virtual ~ImplementationProvider() noexcept = default;
  /// Returns implementation links for `params`.
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getImplementation(const workspace::Document &document,
                    const ::lsp::ImplementationParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token) const = 0;
};

} // namespace pegium
