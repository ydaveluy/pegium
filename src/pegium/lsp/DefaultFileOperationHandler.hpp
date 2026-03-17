#pragma once

#include <pegium/lsp/FileOperationHandler.hpp>
#include <pegium/services/DefaultSharedLspService.hpp>

namespace pegium::lsp {

class DefaultFileOperationHandler : public FileOperationHandler,
                                    protected services::DefaultSharedLspService {
public:
  explicit DefaultFileOperationHandler(services::SharedServices &sharedServices);
  ~DefaultFileOperationHandler() override = default;

  [[nodiscard]] bool supportsDidCreateFiles() const noexcept override;
  [[nodiscard]] bool supportsDidRenameFiles() const noexcept override;
  [[nodiscard]] bool supportsDidDeleteFiles() const noexcept override;

  [[nodiscard]] const ::lsp::FileOperationOptions &
  fileOperationOptions() const noexcept override;

  void didCreateFiles(const ::lsp::CreateFilesParams &params) override;
  void didRenameFiles(const ::lsp::RenameFilesParams &params) override;
  void didDeleteFiles(const ::lsp::DeleteFilesParams &params) override;

private:
  ::lsp::FileOperationOptions _fileOperationOptions;
};

} // namespace pegium::lsp
