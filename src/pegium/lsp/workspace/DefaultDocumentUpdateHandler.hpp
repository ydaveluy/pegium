#pragma once

#include <pegium/lsp/workspace/DocumentUpdateHandler.hpp>
#include <pegium/lsp/services/DefaultSharedLspService.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/Event.hpp>

#include <lsp/types.h>
#include <vector>

namespace pegium {

/// Default bridge between text-document events and workspace rebuilds.
class DefaultDocumentUpdateHandler : public DocumentUpdateHandler,
                                     protected DefaultSharedLspService {
public:
  /// Binds the handler to the shared LSP services.
  explicit DefaultDocumentUpdateHandler(pegium::SharedServices &sharedServices);
  ~DefaultDocumentUpdateHandler() override;

  [[nodiscard]] bool supportsDidSaveDocument() const noexcept override {
    return false;
  }

  [[nodiscard]] bool supportsWillSaveDocument() const noexcept override {
    return false;
  }

  [[nodiscard]] bool
  supportsWillSaveDocumentWaitUntil() const noexcept override {
    return false;
  }

  void didChangeContent(const TextDocumentChangeEvent &event) override;
  void didChangeWatchedFiles(
      const ::lsp::DidChangeWatchedFilesParams &params) override;
  /// Subscribes to watched-file notifications forwarded by this handler.
  utils::ScopedDisposable onWatchedFilesChange(
      const std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>
          &listener) override;

protected:
  /// Returns the file watchers that should be registered after initialize.
  [[nodiscard]] virtual std::vector<::lsp::FileSystemWatcher>
  getWatchers() const;

private:
  void registerFileWatcher();
  void fireDocumentUpdate(
      std::vector<workspace::DocumentId> changedDocumentIds,
      std::vector<workspace::DocumentId> deletedDocumentIds);
  void applyDocumentUpdate(
      std::vector<workspace::DocumentId> changedDocumentIds,
      std::vector<workspace::DocumentId> deletedDocumentIds,
      const utils::CancellationToken &cancelToken);

  utils::EventEmitter<::lsp::DidChangeWatchedFilesParams>
      _onWatchedFilesChange;
  bool _canRegisterWatchedFiles = false;
  utils::ScopedDisposable _onInitializeSubscription;
  utils::ScopedDisposable _onInitializedSubscription;
};

} // namespace pegium
