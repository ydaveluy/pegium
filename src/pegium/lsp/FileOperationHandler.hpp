#pragma once

#include <optional>

#include <lsp/types.h>

namespace pegium::services {
struct SharedServices;
}

namespace pegium::lsp {
class FileOperationHandler {
public:
  virtual ~FileOperationHandler() noexcept = default;

  [[nodiscard]] virtual bool supportsDidCreateFiles() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidRenameFiles() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsDidDeleteFiles() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsWillCreateFiles() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsWillRenameFiles() const noexcept {
    return false;
  }

  [[nodiscard]] virtual bool supportsWillDeleteFiles() const noexcept {
    return false;
  }

  virtual void didCreateFiles(const ::lsp::CreateFilesParams &params) {
    (void)params;
  }
  virtual void didRenameFiles(const ::lsp::RenameFilesParams &params) {
    (void)params;
  }
  virtual void didDeleteFiles(const ::lsp::DeleteFilesParams &params) {
    (void)params;
  }
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willCreateFiles(const ::lsp::CreateFilesParams &params) {
    (void)params;
    return std::nullopt;
  }
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willRenameFiles(const ::lsp::RenameFilesParams &params) {
    (void)params;
    return std::nullopt;
  }
  [[nodiscard]] virtual std::optional<::lsp::WorkspaceEdit>
  willDeleteFiles(const ::lsp::DeleteFilesParams &params) {
    (void)params;
    return std::nullopt;
  }

  [[nodiscard]] virtual const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept = 0;
};

} // namespace pegium::lsp
