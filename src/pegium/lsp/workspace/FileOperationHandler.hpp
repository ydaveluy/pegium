#pragma once

#include <optional>

#include <lsp/types.h>

namespace pegium {
struct SharedServices;
}

namespace pegium {
/// Handles workspace file operations exposed through the LSP.
class FileOperationHandler {
public:
  virtual ~FileOperationHandler() noexcept = default;

  /// Handles file creation notifications.
  virtual void didCreateFiles(const ::lsp::CreateFilesParams &params) {
    (void)params;
  }
  /// Handles file rename notifications.
  virtual void didRenameFiles(const ::lsp::RenameFilesParams &params) {
    (void)params;
  }
  /// Handles file deletion notifications.
  virtual void didDeleteFiles(const ::lsp::DeleteFilesParams &params) {
    (void)params;
  }
  /// Returns edits to apply before file creation, when supported.
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willCreateFiles(const ::lsp::CreateFilesParams &params) {
    (void)params;
    return std::nullopt;
  }
  /// Returns edits to apply before file renames, when supported.
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willRenameFiles(const ::lsp::RenameFilesParams &params) {
    (void)params;
    return std::nullopt;
  }
  /// Returns edits to apply before file deletion, when supported.
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willDeleteFiles(const ::lsp::DeleteFilesParams &params) {
    (void)params;
    return std::nullopt;
  }

  /// Returns the advertised workspace file-operation capabilities.
  [[nodiscard]] virtual const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept = 0;
};

} // namespace pegium
