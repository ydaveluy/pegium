#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides type-definition targets for one document position.
class TypeDefinitionProvider {
public:
  virtual ~TypeDefinitionProvider() noexcept = default;
  /// Returns type-definition links for `params`.
  virtual std::optional<std::vector<::lsp::LocationLink>>
  getTypeDefinition(const workspace::Document &document,
                    const ::lsp::TypeDefinitionParams &params,
                    const utils::CancellationToken &cancelToken =
                        utils::default_cancel_token) const = 0;
};

} // namespace pegium
