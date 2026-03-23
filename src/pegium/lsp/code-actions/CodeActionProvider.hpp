#pragma once

#include <optional>
#include <vector>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides code actions for one document range and diagnostic context.
class CodeActionProvider {
public:
  virtual ~CodeActionProvider() noexcept = default;
  /// Returns the code actions available for `params`, or `std::nullopt` when unsupported.
  virtual std::optional<
      std::vector<::lsp::OneOf<::lsp::Command, ::lsp::CodeAction>>>
  getCodeActions(const workspace::Document &document,
                 const ::lsp::CodeActionParams &params,
                 const utils::CancellationToken &cancelToken =
                     utils::default_cancel_token) const = 0;
};

} // namespace pegium
