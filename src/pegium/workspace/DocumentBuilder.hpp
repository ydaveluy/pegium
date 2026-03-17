#pragma once

#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/Event.hpp>
#include <pegium/validation/ValidationOptions.hpp>
#include <pegium/workspace/Documents.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::services {
class ServiceRegistry;
}

namespace pegium::workspace {

struct BuildOptions {
  bool eagerLinking = true;
  validation::ValidationOptions validation = [] {
    validation::ValidationOptions options;
    options.enabled = false;
    return options;
  }();
};

struct DocumentUpdateResult {
  std::vector<std::shared_ptr<Document>> rebuiltDocuments;
  std::vector<DocumentId> deletedDocumentIds;
};

struct DocumentUpdateEvent {
  std::vector<DocumentId> changedDocumentIds;
  std::vector<DocumentId> deletedDocumentIds;
};

struct DocumentBuildPhaseEvent {
  DocumentState targetState = DocumentState::Changed;
  std::vector<std::shared_ptr<Document>> builtDocuments;
};

struct DocumentPhaseEvent {
  DocumentState targetState = DocumentState::Changed;
  std::shared_ptr<Document> builtDocument;
};

struct DocumentBuildState {
  bool completed = false;
  BuildOptions options{};
  struct Result {
    std::vector<std::string> validationCategories;
  };
  std::optional<Result> result;
};

class DocumentBuilder {
public:
  virtual ~DocumentBuilder() noexcept = default;

  [[nodiscard]] virtual BuildOptions &updateBuildOptions() noexcept = 0;
  [[nodiscard]] virtual const BuildOptions &
  updateBuildOptions() const noexcept = 0;

  [[nodiscard]] virtual bool
  build(std::span<const std::shared_ptr<Document>> documents,
        const BuildOptions &options = {},
        utils::CancellationToken cancelToken = {}) const = 0;

  [[nodiscard]] virtual DocumentUpdateResult
  update(std::span<const DocumentId> changedDocumentIds,
         std::span<const DocumentId> deletedDocumentIds,
         utils::CancellationToken cancelToken = {},
         bool rebuildDocuments = true) const = 0;

  virtual utils::ScopedDisposable
  onUpdate(std::function<void(std::span<const DocumentId> changedDocumentIds,
                              std::span<const DocumentId> deletedDocumentIds)>
               listener) = 0;
  virtual utils::ScopedDisposable onBuildPhase(
      DocumentState targetState,
      std::function<void(std::span<const std::shared_ptr<Document>> builtDocuments)>
          listener) = 0;
  virtual utils::ScopedDisposable
  onDocumentPhase(DocumentState targetState,
                  std::function<void(const std::shared_ptr<Document> &)>
                      listener) = 0;

  virtual void
  waitUntil(DocumentState state,
            utils::CancellationToken cancelToken = {}) const = 0;
  virtual void
  waitUntil(DocumentState state, DocumentId documentId,
            utils::CancellationToken cancelToken = {}) const = 0;

  virtual void resetToState(Document &document, DocumentState state) const = 0;
};

} // namespace pegium::workspace
