#pragma once

#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides code lenses for one document.
class CodeLensProvider {
public:
  virtual ~CodeLensProvider() noexcept = default;
  /// Returns the code lenses to show for `document`.
  virtual std::vector<::lsp::CodeLens>
  provideCodeLens(const workspace::Document &document,
                  const ::lsp::CodeLensParams &params,
                  const utils::CancellationToken &cancelToken =
                      utils::default_cancel_token) const = 0;
};

} // namespace pegium
