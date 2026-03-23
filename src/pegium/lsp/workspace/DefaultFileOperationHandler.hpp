#pragma once

#include <pegium/lsp/workspace/FileOperationHandler.hpp>
#include <pegium/lsp/services/DefaultSharedLspService.hpp>

namespace pegium {

/// Default file-operation handler that mirrors workspace file events.
class DefaultFileOperationHandler : public FileOperationHandler,
                                    protected DefaultSharedLspService {
public:
  /// Binds the handler to the shared LSP services.
  explicit DefaultFileOperationHandler(pegium::SharedServices &sharedServices);
  ~DefaultFileOperationHandler() override = default;

  /// Returns the capabilities advertised for workspace file operations.
  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override;

  void didCreateFiles(const ::lsp::CreateFilesParams &params) override;
  void didRenameFiles(const ::lsp::RenameFilesParams &params) override;
  void didDeleteFiles(const ::lsp::DeleteFilesParams &params) override;

private:
  ::lsp::FileOperationOptions _fileOperationOptions;
};

} // namespace pegium
