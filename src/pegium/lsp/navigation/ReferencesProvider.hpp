#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides incoming references of one symbol.
class ReferencesProvider {
public:
  virtual ~ReferencesProvider() noexcept = default;
  /// Returns the references selected by `params`.
  virtual std::vector<::lsp::Location>
  findReferences(const workspace::Document &document,
                 const ::lsp::ReferenceParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium
