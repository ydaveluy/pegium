#pragma once

#include <memory>

#include <pegium/lsp/DocumentUpdateHandler.hpp>
#include <pegium/services/DefaultSharedLspService.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/Event.hpp>

#include <lsp/types.h>

namespace pegium::lsp {

class DefaultDocumentUpdateHandler : public DocumentUpdateHandler,
                                     protected services::DefaultSharedLspService {
public:
  explicit DefaultDocumentUpdateHandler(services::SharedServices &sharedServices);
  ~DefaultDocumentUpdateHandler() override;

  [[nodiscard]] bool supportsDidChangeContent() const noexcept override;
  [[nodiscard]] bool supportsDidChangeWatchedFiles() const noexcept override;

  void didOpenDocument(const TextDocumentChangeEvent &event) override;
  void didChangeContent(const TextDocumentChangeEvent &event) override;
  void didSaveDocument(const TextDocumentChangeEvent &event) override;
  void didCloseDocument(const TextDocumentChangeEvent &event) override;
  void didChangeWatchedFiles(
      const ::lsp::DidChangeWatchedFilesParams &params) override;
  utils::ScopedDisposable onWatchedFilesChange(
      std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>
          listener) override;

private:
  class Impl;

  void scheduleDocumentUpdate(workspace::DocumentId documentId);
  void scheduleWorkspaceUpdate(
      std::vector<workspace::DocumentId> changedDocumentIds,
      std::vector<workspace::DocumentId> deletedDocumentIds);
  void applyDocumentUpdate(
      std::vector<workspace::DocumentId> changedDocumentIds,
      std::vector<workspace::DocumentId> deletedDocumentIds,
                           const utils::CancellationToken &cancelToken);

  std::unique_ptr<Impl> _impl;
  utils::EventEmitter<::lsp::DidChangeWatchedFilesParams>
      _onWatchedFilesChange;
};

} // namespace pegium::lsp
