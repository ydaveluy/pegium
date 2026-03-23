#pragma once

#include <optional>

#include <lsp/types.h>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium {

/// Provides rename and prepare-rename operations.
class RenameProvider {
public:
  virtual ~RenameProvider() noexcept = default;

  /// Returns the workspace edit needed to rename the selected symbol.
  virtual std::optional<::lsp::WorkspaceEdit>
  rename(const workspace::Document &document,
         const ::lsp::RenameParams &params,
         const utils::CancellationToken &cancelToken =
             utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return std::nullopt;
  }

  /// Returns the range and placeholder that can be renamed at `params`.
  virtual std::optional<::lsp::PrepareRenameResult>
  prepareRename(const workspace::Document &document,
                const ::lsp::TextDocumentPositionParams &params,
                const utils::CancellationToken &cancelToken =
                    utils::default_cancel_token) const {
    (void)document;
    (void)params;
    (void)cancelToken;
    return std::nullopt;
  }
};

} // namespace pegium
