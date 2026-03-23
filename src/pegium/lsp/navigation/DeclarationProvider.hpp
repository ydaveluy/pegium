#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides declaration targets for one document position.
class DeclarationProvider {
public:
  virtual ~DeclarationProvider() noexcept = default;
  /// Returns declaration links for `params`.
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getDeclaration(const workspace::Document &document,
                 const ::lsp::DeclarationParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium
