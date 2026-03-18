#include <pegium/lsp/DefaultDocumentUpdateHandler.hpp>

#include <condition_variable>
#include <array>
#include <deque>
#include <future>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <utility>
#include <vector>

#include <pegium/services/SharedServices.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/UriUtils.hpp>

namespace pegium::lsp {

namespace {

bool merge_validation_options_for_language(
    const services::SharedServices &sharedServices, std::string_view languageId,
    workspace::BuildOptions &options) {
  if (sharedServices.workspace.configurationProvider == nullptr ||
      languageId.empty()) {
    return false;
  }
  if (const auto validation =
          sharedServices.workspace.configurationProvider->getConfiguration(
              languageId, "validation");
      validation.has_value()) {
    (void)workspace::readValidationOptions(*validation, options.validation);
    return true;
  }
  return false;
}

workspace::DocumentId
select_update_document_id(std::span<const workspace::DocumentId> changedDocumentIds,
                          std::span<const workspace::DocumentId> deletedDocumentIds) {
  if (!changedDocumentIds.empty()) {
    return changedDocumentIds.front();
  }
  if (!deletedDocumentIds.empty()) {
    return deletedDocumentIds.front();
  }
  return workspace::InvalidDocumentId;
}

bool merge_validation_options_for_document(
    const services::SharedServices &sharedServices,
    workspace::DocumentId documentId, workspace::BuildOptions &options) {
  auto *documents = sharedServices.workspace.documents.get();
  if (documents == nullptr || documentId == workspace::InvalidDocumentId) {
    return false;
  }
  if (const auto document = documents->getDocument(documentId);
      document != nullptr && !document->languageId.empty()) {
    return merge_validation_options_for_language(sharedServices,
                                                 document->languageId, options);
  }
  return false;
}

} // namespace

class DefaultDocumentUpdateHandler::Impl {
public:
  using Callback =
      std::function<void(const utils::CancellationToken &cancelToken)>;

  struct EnqueueOptions {
    workspace::DocumentId documentId = workspace::InvalidDocumentId;
    bool supersedeDocument = false;
  };

  Impl() : _worker([this](std::stop_token stopToken) { run(stopToken); }) {}

  ~Impl() { requestStop(); }

  Impl(const Impl &) = delete;
  Impl &operator=(const Impl &) = delete;

  [[nodiscard]] std::future<void> enqueue(Callback callback) {
    return enqueue(std::move(callback), EnqueueOptions{});
  }

  [[nodiscard]] std::future<void> enqueue(Callback callback,
                                          EnqueueOptions options) {
    if (!callback) {
      return makeReadyFuture();
    }

    QueuedTask task;
    auto future = std::future<void>{};
    {
      std::scoped_lock lock(_mutex);
      if (_stopping) {
        return makeReadyFuture();
      }

      task.callback = std::move(callback);
      task.documentId = options.documentId;

      if (task.documentId != workspace::InvalidDocumentId) {
        auto &latestGeneration = _latestGenerationByDocumentId[task.documentId];
        if (options.supersedeDocument) {
          ++latestGeneration;

          auto inFlight = _inFlightByDocumentId.find(task.documentId);
          if (inFlight != _inFlightByDocumentId.end() &&
              inFlight->second != nullptr) {
            inFlight->second->request_stop();
          }

          for (auto &queued : _queue) {
            if (queued.documentId == task.documentId) {
              queued.cancellation.request_stop();
            }
          }
        }
        task.documentGeneration = latestGeneration;
      }

      future = task.completion.get_future();
      _queue.push_back(std::move(task));
    }
    _cv.notify_all();
    return future;
  }

private:
  struct QueuedTask {
    workspace::DocumentId documentId = workspace::InvalidDocumentId;
    std::uint64_t documentGeneration = 0;
    Callback callback;
    utils::CancellationTokenSource cancellation;
    std::promise<void> completion;
  };

  void requestStop() {
    {
      std::scoped_lock lock(_mutex);
      if (_stopping) {
        return;
      }
      _stopping = true;

      for (auto &task : _queue) {
        task.cancellation.request_stop();
      }
      for (auto &[documentId, source] : _inFlightByDocumentId) {
        (void)documentId;
        if (source != nullptr) {
          source->request_stop();
        }
      }
    }
    _cv.notify_all();
  }

  void run(std::stop_token stopToken) {
    while (true) {
      QueuedTask task;
      {
        std::unique_lock lock(_mutex);
        _cv.wait(lock, [this, &stopToken]() {
          return _stopping || stopToken.stop_requested() || !_queue.empty();
        });

        if ((_stopping || stopToken.stop_requested()) && _queue.empty()) {
          return;
        }

        task = std::move(_queue.front());
        _queue.pop_front();
        ++_inFlightCount;
        if (task.documentId != workspace::InvalidDocumentId) {
          _inFlightByDocumentId[task.documentId] = &task.cancellation;
        }
      }

      bool skipTask = task.cancellation.stop_requested();
      {
        std::scoped_lock lock(_mutex);
        skipTask = skipTask || _stopping || stopToken.stop_requested();
        if (!skipTask && task.documentId != workspace::InvalidDocumentId) {
          const auto latestIt =
              _latestGenerationByDocumentId.find(task.documentId);
          if (latestIt != _latestGenerationByDocumentId.end() &&
              task.documentGeneration < latestIt->second) {
            skipTask = true;
          }
        }
      }

      try {
        if (!skipTask) {
          task.callback(task.cancellation.get_token());
        }
        task.completion.set_value();
      } catch (const utils::OperationCancelled &) {
        task.completion.set_value();
      } catch (...) {
        task.completion.set_exception(std::current_exception());
      }

      {
        std::scoped_lock lock(_mutex);
        if (task.documentId != workspace::InvalidDocumentId) {
          const auto inFlight = _inFlightByDocumentId.find(task.documentId);
          if (inFlight != _inFlightByDocumentId.end() &&
              inFlight->second == &task.cancellation) {
            _inFlightByDocumentId.erase(inFlight);
          }
        }
        if (_inFlightCount > 0) {
          --_inFlightCount;
        }
      }
    }
  }

  [[nodiscard]] static std::future<void> makeReadyFuture() {
    std::promise<void> promise;
    promise.set_value();
    return promise.get_future();
  }

  std::mutex _mutex;
  std::condition_variable _cv;
  std::deque<QueuedTask> _queue;
  std::unordered_map<workspace::DocumentId, std::uint64_t>
      _latestGenerationByDocumentId;
  std::unordered_map<workspace::DocumentId, utils::CancellationTokenSource *>
      _inFlightByDocumentId;
  std::uint64_t _inFlightCount = 0;
  bool _stopping = false;
  std::jthread _worker;
};

DefaultDocumentUpdateHandler::DefaultDocumentUpdateHandler(
    services::SharedServices &sharedServices)
    : services::DefaultSharedLspService(sharedServices),
      _impl(std::make_unique<Impl>()) {}

DefaultDocumentUpdateHandler::~DefaultDocumentUpdateHandler() = default;

bool DefaultDocumentUpdateHandler::supportsDidChangeContent() const noexcept {
  return true;
}

bool DefaultDocumentUpdateHandler::supportsDidChangeWatchedFiles() const noexcept {
  return true;
}

namespace {

bool is_redundant_text_snapshot(
    workspace::Documents *documents,
    const std::shared_ptr<const workspace::TextDocument> &textDocument) {
  if (documents == nullptr || textDocument == nullptr || textDocument->uri.empty()) {
    return false;
  }

  const auto documentId = documents->getDocumentId(textDocument->uri);
  if (documentId == workspace::InvalidDocumentId) {
    return false;
  }

  const auto document = documents->getDocument(documentId);
  if (document == nullptr) {
    return false;
  }

  return document->text() == textDocument->text() &&
         document->languageId == textDocument->languageId;
}

} // namespace

void DefaultDocumentUpdateHandler::didOpenDocument(
    const TextDocumentChangeEvent &event) {
  if (!event.document || event.document->uri.empty()) {
    return;
  }
  auto *documents = sharedServices.workspace.documents.get();
  if (documents == nullptr) {
    return;
  }
  const auto documentId = documents->getOrCreateDocumentId(event.document->uri);
  if (documentId == workspace::InvalidDocumentId) {
    return;
  }
  scheduleDocumentUpdate(documentId);
}

void DefaultDocumentUpdateHandler::didChangeContent(
    const TextDocumentChangeEvent &event) {
  if (!event.document || event.document->uri.empty()) {
    return;
  }
  auto *documents = sharedServices.workspace.documents.get();
  if (documents == nullptr) {
    return;
  }
  if (is_redundant_text_snapshot(documents, event.document)) {
    return;
  }
  const auto documentId = documents->getOrCreateDocumentId(event.document->uri);
  if (documentId == workspace::InvalidDocumentId) {
    return;
  }
  scheduleDocumentUpdate(documentId);
}

void DefaultDocumentUpdateHandler::didSaveDocument(
    const TextDocumentChangeEvent &event) {
  if (!event.document || event.document->uri.empty()) {
    return;
  }
}

void DefaultDocumentUpdateHandler::didCloseDocument(
    const TextDocumentChangeEvent &event) {
  if (!event.document || event.document->uri.empty()) {
    return;
  }
}

void DefaultDocumentUpdateHandler::scheduleDocumentUpdate(
    workspace::DocumentId documentId) {
  (void)_impl->enqueue(
      [this, documentId](
          const utils::CancellationToken &cancelToken) mutable {
        const std::array<workspace::DocumentId, 1> changedDocumentIds{documentId};
        applyDocumentUpdate(std::vector<workspace::DocumentId>(
                                changedDocumentIds.begin(),
                                changedDocumentIds.end()),
                            {}, cancelToken);
      },
      {.documentId = documentId, .supersedeDocument = true});
}

void DefaultDocumentUpdateHandler::scheduleWorkspaceUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds) {
  const auto scheduledDocumentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
  (void)_impl->enqueue(
      [this, changedDocumentIds = std::move(changedDocumentIds),
       deletedDocumentIds = std::move(deletedDocumentIds)](
          const utils::CancellationToken &cancelToken) {
        applyDocumentUpdate(changedDocumentIds, deletedDocumentIds, cancelToken);
      },
      {.documentId = scheduledDocumentId,
       .supersedeDocument = scheduledDocumentId != workspace::InvalidDocumentId});
}

void DefaultDocumentUpdateHandler::applyDocumentUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds,
    const utils::CancellationToken &cancelToken) {
  auto *documentBuilder = sharedServices.workspace.documentBuilder.get();
  if (documentBuilder == nullptr) {
    return;
  }
  const auto documentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
  workspace::run_with_workspace_write(
      sharedServices.workspace.workspaceLock.get(), cancelToken,
      [this, documentBuilder, &changedDocumentIds, &deletedDocumentIds,
       &cancelToken, documentId]() {
        auto originalOptions = documentBuilder->updateBuildOptions();
        auto effectiveOptions = originalOptions;
        const bool hasValidationOverride =
            merge_validation_options_for_document(sharedServices, documentId,
                                                 effectiveOptions);
        if (hasValidationOverride) {
          documentBuilder->updateBuildOptions() = effectiveOptions;
        }

        try {
          (void)documentBuilder->update(changedDocumentIds, deletedDocumentIds,
                                        cancelToken);
        } catch (...) {
          if (hasValidationOverride) {
            documentBuilder->updateBuildOptions() = std::move(originalOptions);
          }
          throw;
        }

        if (hasValidationOverride) {
          documentBuilder->updateBuildOptions() = std::move(originalOptions);
        }
      });
}

void DefaultDocumentUpdateHandler::didChangeWatchedFiles(
    const ::lsp::DidChangeWatchedFilesParams &params) {
  _onWatchedFilesChange.emit(params);

  std::vector<workspace::DocumentId> changedDocumentIds;
  std::vector<workspace::DocumentId> deletedDocumentIds;
  std::unordered_set<workspace::DocumentId> seenChanged;
  std::unordered_set<workspace::DocumentId> seenDeleted;
  auto *documents = sharedServices.workspace.documents.get();
  if (documents == nullptr) {
    return;
  }

  for (const auto &change : params.changes) {
    const auto uri = utils::normalize_uri(change.uri.toString());
    if (uri.empty()) {
      continue;
    }

    if (change.type == ::lsp::FileChangeType::Deleted) {
      if (const auto documentId = documents->getDocumentId(uri);
          documentId != workspace::InvalidDocumentId &&
          seenDeleted.insert(documentId).second) {
        deletedDocumentIds.push_back(documentId);
      }
      continue;
    }

    if (const auto documentId = documents->getOrCreateDocumentId(uri);
        documentId != workspace::InvalidDocumentId &&
        !seenDeleted.contains(documentId) &&
        seenChanged.insert(documentId).second) {
      changedDocumentIds.push_back(documentId);
    }
  }

  if (changedDocumentIds.empty() && deletedDocumentIds.empty()) {
    return;
  }

  scheduleWorkspaceUpdate(std::move(changedDocumentIds),
                          std::move(deletedDocumentIds));
}

utils::ScopedDisposable DefaultDocumentUpdateHandler::onWatchedFilesChange(
    std::function<void(const ::lsp::DidChangeWatchedFilesParams &)> listener) {
  return _onWatchedFilesChange.on(std::move(listener));
}

} // namespace pegium::lsp
