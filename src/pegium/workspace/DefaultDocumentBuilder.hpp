#pragma once

#include <condition_variable>
#include <mutex>
#include <unordered_map>

#include <pegium/services/DefaultSharedCoreService.hpp>
#include <pegium/workspace/DocumentBuilder.hpp>

namespace pegium::workspace {

class DefaultDocumentBuilder : public DocumentBuilder,
                               protected services::DefaultSharedCoreService {
public:
  explicit DefaultDocumentBuilder(services::SharedCoreServices &sharedServices);

  [[nodiscard]] BuildOptions &updateBuildOptions() noexcept override;
  [[nodiscard]] const BuildOptions &
  updateBuildOptions() const noexcept override;

  [[nodiscard]] bool
  build(std::span<const std::shared_ptr<Document>> documents,
        const BuildOptions &options = {},
        utils::CancellationToken cancelToken = {}) const override;

  [[nodiscard]] DocumentUpdateResult
  update(std::span<const DocumentId> changedDocumentIds,
         std::span<const DocumentId> deletedDocumentIds,
         utils::CancellationToken cancelToken = {},
         bool rebuildDocuments = true) const override;

  utils::ScopedDisposable
  onUpdate(std::function<void(std::span<const DocumentId> changedDocumentIds,
                              std::span<const DocumentId> deletedDocumentIds)>
               listener) override;
  utils::ScopedDisposable onBuildPhase(
      DocumentState targetState,
      std::function<void(std::span<const std::shared_ptr<Document>> builtDocuments)>
          listener) override;
  utils::ScopedDisposable
  onDocumentPhase(DocumentState targetState,
                  std::function<void(const std::shared_ptr<Document> &)>
                      listener) override;

  void waitUntil(DocumentState state,
                 utils::CancellationToken cancelToken = {}) const override;
  void waitUntil(DocumentState state, DocumentId documentId,
                 utils::CancellationToken cancelToken = {}) const override;

  void resetToState(Document &document, DocumentState state) const override;

private:
  [[nodiscard]] bool
  buildInternal(std::span<const std::shared_ptr<Document>> documents,
                const BuildOptions &options,
                utils::CancellationToken cancelToken,
                bool emitUpdateEvent) const;

  BuildOptions _updateBuildOptions{};

  mutable std::mutex _stateMutex;
  mutable std::condition_variable _stateCv;
  mutable DocumentState _currentState = DocumentState::Changed;
  mutable std::unordered_map<DocumentId, DocumentBuildState>
      _buildStateByDocumentId;

  utils::EventEmitter<DocumentUpdateEvent> _onUpdate;
  utils::EventEmitter<DocumentBuildPhaseEvent> _onBuildPhase;
  utils::EventEmitter<DocumentPhaseEvent> _onDocumentPhase;
};

} // namespace pegium::workspace
