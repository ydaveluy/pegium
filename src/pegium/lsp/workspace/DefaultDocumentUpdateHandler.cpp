#include <pegium/lsp/workspace/DefaultDocumentUpdateHandler.hpp>

#include <chrono>
#include <future>
#include <mutex>
#include <unordered_set>
#include <utility>
#include <vector>

#include <lsp/messages.h>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/lsp/runtime/internal/RuntimeObservability.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/FileSystemProvider.hpp>
#include <pegium/core/workspace/TextDocumentProvider.hpp>
#include <pegium/core/workspace/WorkspaceManager.hpp>

namespace pegium {

namespace {

// Compares the new client snapshot against the last built text. MUST be called
// while holding the workspace write lock (it reads the Document's
// parseResult/textDocument, which a build reassigns), i.e. from inside the write
// action — never off-lock and never via a separate read lock, which would
// deadlock behind an in-flight build that only this write supersedes.
bool is_redundant_text_snapshot(const workspace::Documents &documents,
                                const workspace::TextDocument &textDocument) {
  const auto document = documents.getDocument(textDocument.uri());
  if (document == nullptr) {
    return false;
  }
  if (document->parseResult.cst != nullptr) {
    return document->parseResult.cst->getText() == textDocument.getText();
  }
  return document->textDocument().getText() == textDocument.getText();
}

bool merge_validation_options_for_language(
    const pegium::SharedServices &sharedServices, std::string_view languageId,
    workspace::BuildOptions &options) {
  if (languageId.empty()) {
    return false;
  }
  if (const auto validation =
          sharedServices.workspace.configurationProvider->getConfiguration(
              languageId, "validation");
      validation.has_value()) {
    (void)workspace::readValidationOption(*validation, options.validation);
    return true;
  }
  return false;
}

workspace::DocumentId select_update_document_id(
    std::span<const workspace::DocumentId> changedDocumentIds,
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
    const pegium::SharedServices &sharedServices,
    workspace::DocumentId documentId, workspace::BuildOptions &options) {
  const auto &documents = *sharedServices.workspace.documents;
  if (const auto document = documents.getDocument(documentId);
      document != nullptr) {
    const auto &languageId = document->textDocument().languageId();
    if (languageId.empty()) {
      return false;
    }
    return merge_validation_options_for_language(sharedServices,
                                                 languageId, options);
  }
  return false;
}

void publish_document_update_dispatch_failed(
    const pegium::SharedServices &sharedServices,
    workspace::DocumentId documentId, std::string message) {
  observability::Observation observation{
      .severity = observability::ObservationSeverity::Error,
      .code = observability::ObservationCode::DocumentUpdateDispatchFailed,
      .message = std::move(message)};
  observation.documentId = documentId;
  if (const auto document =
          sharedServices.workspace.documents->getDocument(documentId);
      document != nullptr) {
    observation.uri = document->uri;
    observation.languageId = document->textDocument().languageId();
    observation.state = document->state;
  } else if (const auto uri =
                 sharedServices.workspace.documents->getDocumentUri(documentId);
             !uri.empty()) {
    observation.uri = uri;
  }
  sharedServices.observabilitySink->publish(observation);
}

// Expands one changed URI into the concrete source files it affects. A URI that
// already backs a managed document or an open text document maps to itself; an
// unknown URI is stat'd so a directory fans out to the source files it contains
// and a stray non-source entry is dropped.
std::vector<std::string>
find_changed_uris(const pegium::SharedServices &sharedServices,
                  const std::string &changedUri) {
  const auto &documents = *sharedServices.workspace.documents;
  if (const auto *textDocuments = sharedServices.workspace.textDocuments.get();
      documents.hasDocument(changedUri) ||
      (textDocuments != nullptr &&
       textDocuments->getNormalized(changedUri) != nullptr)) {
    return {changedUri};
  }

  const auto *fileSystemProvider =
      sharedServices.workspace.fileSystemProvider.get();
  const auto *workspaceManager = sharedServices.workspace.workspaceManager.get();
  if (fileSystemProvider == nullptr || workspaceManager == nullptr) {
    return {};
  }
  try {
    const auto stat = fileSystemProvider->stat(changedUri);
    if (stat.isDirectory) {
      return workspaceManager->searchFolder(changedUri);
    }
    if (workspaceManager->shouldIncludeEntry(stat)) {
      return {changedUri};
    }
  } catch (const std::exception &) {
    // The file type cannot be determined, so the change is discarded.
  }
  return {};
}

::lsp::RegistrationParams make_watched_files_registration(
    std::vector<::lsp::FileSystemWatcher> watchers) {
  ::lsp::DidChangeWatchedFilesRegistrationOptions options{};
  options.watchers = std::move(watchers);

  ::lsp::Registration registration{};
  registration.id = "pegium.workspace.didChangeWatchedFiles";
  registration.method = std::string(
      ::lsp::notifications::Workspace_DidChangeWatchedFiles::Method);
  registration.registerOptions = ::lsp::toJson(std::move(options));

  ::lsp::RegistrationParams params{};
  params.registrations.push_back(std::move(registration));
  return params;
}

} // namespace

DefaultDocumentUpdateHandler::DefaultDocumentUpdateHandler(
    pegium::SharedServices &sharedServices)
    : DefaultSharedLspService(sharedServices) {
  if (sharedServices.lsp.languageServer == nullptr) {
    return;
  }

  _onInitializeSubscription = sharedServices.lsp.languageServer->onInitialize(
      [this](const ::lsp::InitializeParams &params) {
        _canRegisterWatchedFiles =
            params.capabilities.workspace.has_value() &&
            params.capabilities.workspace->didChangeWatchedFiles.has_value() &&
            params.capabilities.workspace->didChangeWatchedFiles
                ->dynamicRegistration.value_or(false);
      });
  _onInitializedSubscription =
      sharedServices.lsp.languageServer->onInitialized(
          [this](const ::lsp::InitializedParams &) {
            if (_canRegisterWatchedFiles) {
              registerFileWatcher();
            }
          });
}

void DefaultDocumentUpdateHandler::quiesce() {
  // Quiesce in-flight update dispatches before whatever they capture is torn
  // down: our members and, through the shared services, the workspace lock — and
  // at LSP teardown the message handler the diagnostics listeners publish into,
  // since those listeners run on these dispatch threads. The WorkspaceLock
  // requires owners to quiesce their handlers before destruction; a dispatch
  // resuming from write()/ready() on destroyed state would be a use-after-free.
  // Cancel any pending/in-progress write so a dispatch blocked in write()
  // unblocks, then wait for every dispatch to finish. Idempotent.
  if (shared.workspace.workspaceLock != nullptr) {
    shared.workspace.workspaceLock->cancelWrite();
  }
  std::vector<std::future<void>> dispatches;
  {
    std::scoped_lock lock(_dispatchMutex);
    dispatches = std::move(_dispatches);
  }
  for (auto &dispatch : dispatches) {
    if (dispatch.valid()) {
      dispatch.wait();
    }
  }
}

DefaultDocumentUpdateHandler::~DefaultDocumentUpdateHandler() { quiesce(); }

std::vector<::lsp::FileSystemWatcher>
DefaultDocumentUpdateHandler::getWatchers() const {
  ::lsp::FileSystemWatcher watcher{};
  watcher.globPattern = ::lsp::GlobPattern{"**/*"};
  return {std::move(watcher)};
}

void DefaultDocumentUpdateHandler::registerFileWatcher() {
  auto *languageClient = shared.lsp.languageClient.get();
  if (languageClient == nullptr) {
    return;
  }

  auto watchers = getWatchers();
  if (watchers.empty()) {
    return;
  }

  observe_background_task(
      shared, "workspace/didChangeWatchedFiles.register",
      languageClient->registerCapability(
          make_watched_files_registration(std::move(watchers))));
}

void DefaultDocumentUpdateHandler::didChangeContent(
    const TextDocumentChangeEvent &event) {
  // event.document is an immutable snapshot of the new client text. Pass it as
  // the redundancy snapshot so the rebuild can be skipped when the text is
  // unchanged — checked under the write lock inside the async task, instead of
  // racing the build off-lock on the message thread.
  auto &documents = *shared.workspace.documents;
  fireDocumentUpdate({documents.getOrCreateDocumentId(event.document->uri())}, {},
                     event.document);
}

void DefaultDocumentUpdateHandler::fireDocumentUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds,
    std::shared_ptr<const workspace::TextDocument> redundantWhenUnchanged) {
  // Capture the reporting document id before the id vectors are moved into the
  // worker (and then the write action), so a failure can still identify the
  // document instead of reporting on moved-from (empty) vectors.
  const auto reportDocumentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
  auto future = std::async(
      std::launch::async,
      [this, reportDocumentId,
       changedDocumentIds = std::move(changedDocumentIds),
       deletedDocumentIds = std::move(deletedDocumentIds),
       redundantWhenUnchanged = std::move(redundantWhenUnchanged)]() mutable {
        try {
          // `ready()` only guarantees that startup documents were discovered
          // and materialized. The tail of the initial build may still be
          // running here and can be superseded by this newer workspace write.
          auto ready = shared.workspace.workspaceManager->ready();
          ready.get();
          auto writeFuture = shared.workspace.workspaceLock->write(
              [this, changedDocumentIds = std::move(changedDocumentIds),
               deletedDocumentIds = std::move(deletedDocumentIds),
               redundantWhenUnchanged = std::move(redundantWhenUnchanged)](
                  const utils::CancellationToken &cancelToken,
                  const workspace::WorkspaceLock::Downgrade &downgrade) mutable {
                // Under the write lock the workspace Document is exclusive, so
                // this redundancy check cannot race the build. It runs here —
                // after this write supersedes any in-flight build — rather than
                // before the write, which would deadlock behind a build that
                // only this write supersedes.
                if (redundantWhenUnchanged != nullptr &&
                    is_redundant_text_snapshot(*shared.workspace.documents,
                                               *redundantWhenUnchanged)) {
                  return;
                }
                applyDocumentUpdate(std::move(changedDocumentIds),
                                    std::move(deletedDocumentIds), cancelToken,
                                    downgrade);
              });
          writeFuture.get();
        } catch (const utils::OperationCancelled &) {
        } catch (const std::exception &error) {
          publish_document_update_dispatch_failed(
              shared, reportDocumentId,
              "Workspace initialization failed. Could not perform document "
              "update: " +
                  std::string(error.what()));
        } catch (...) {
          publish_document_update_dispatch_failed(
              shared, reportDocumentId,
              "Workspace initialization failed. Could not perform document "
              "update.");
        }
      });
  // Keep the dispatch future so the destructor can wait for it; prune the
  // already-finished ones. The task reports its own failures internally, so it
  // does not need observe_background_task here.
  std::scoped_lock lock(_dispatchMutex);
  std::erase_if(_dispatches, [](std::future<void> &dispatch) {
    return dispatch.wait_for(std::chrono::seconds(0)) ==
           std::future_status::ready;
  });
  _dispatches.push_back(std::move(future));
}

void DefaultDocumentUpdateHandler::applyDocumentUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds,
    const utils::CancellationToken &cancelToken,
    const std::function<void()> &downgrade) {
  utils::throw_if_cancelled(cancelToken);

  auto &documentBuilder = *shared.workspace.documentBuilder;

  const auto documentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
  auto originalOptions = documentBuilder.updateBuildOptions();
  auto effectiveOptions = originalOptions;
  // The override swaps a single BuildOptions for the whole batch, so it is only
  // correct for a single-document batch (didChangeContent). A multi-document
  // batch (didChangeWatchedFiles) can span languages, where the first
  // document's validation config must not be forced onto the others; fall back
  // to the builder defaults there.
  const bool singleDocumentBatch =
      changedDocumentIds.size() + deletedDocumentIds.size() == 1;
  const bool hasValidationOverride =
      singleDocumentBatch &&
      merge_validation_options_for_document(shared, documentId, effectiveOptions);
  if (hasValidationOverride) {
    documentBuilder.updateBuildOptions() = effectiveOptions;
  }

  try {
    documentBuilder.update(changedDocumentIds, deletedDocumentIds, cancelToken,
                           downgrade);
  } catch (...) {
    if (hasValidationOverride) {
      documentBuilder.updateBuildOptions() = std::move(originalOptions);
    }
    throw;
  }

  if (hasValidationOverride) {
    documentBuilder.updateBuildOptions() = std::move(originalOptions);
  }
  utils::throw_if_cancelled(cancelToken);
}

void DefaultDocumentUpdateHandler::didChangeWatchedFiles(
    const ::lsp::DidChangeWatchedFilesParams &params) {
  _onWatchedFilesChange.emit(params);

  std::vector<workspace::DocumentId> changedDocumentIds;
  std::vector<workspace::DocumentId> deletedDocumentIds;
  std::unordered_set<workspace::DocumentId> seenChanged;
  std::unordered_set<workspace::DocumentId> seenDeleted;
  auto &documents = *shared.workspace.documents;

  // A deleted URI can point at a directory, so every managed document within
  // its subtree must be removed.
  for (const auto &change : params.changes) {
    if (change.type != ::lsp::FileChangeType::Deleted) {
      continue;
    }
    const auto uri = utils::normalize_uri(change.uri.toString());
    if (uri.empty()) {
      continue;
    }
    for (const auto &document : documents.getDocuments(uri)) {
      if (document != nullptr &&
          document->id != workspace::InvalidDocumentId &&
          seenDeleted.insert(document->id).second) {
        deletedDocumentIds.push_back(document->id);
      }
    }
  }

  // A changed URI can point at a directory or a not-yet-known file, so expand
  // it to the concrete source files it affects before scheduling a rebuild.
  for (const auto &change : params.changes) {
    if (change.type == ::lsp::FileChangeType::Deleted) {
      continue;
    }
    const auto uri = utils::normalize_uri(change.uri.toString());
    if (uri.empty()) {
      continue;
    }
    for (const auto &fileUri : find_changed_uris(shared, uri)) {
      const auto documentId = documents.getOrCreateDocumentId(fileUri);
      if (!seenDeleted.contains(documentId) &&
          seenChanged.insert(documentId).second) {
        changedDocumentIds.push_back(documentId);
      }
    }
  }

  if (changedDocumentIds.empty() && deletedDocumentIds.empty()) {
    return;
  }

  fireDocumentUpdate(std::move(changedDocumentIds),
                     std::move(deletedDocumentIds));
}

utils::ScopedDisposable DefaultDocumentUpdateHandler::onWatchedFilesChange(
    const std::function<void(const ::lsp::DidChangeWatchedFilesParams &)>
        &listener) {
  return _onWatchedFilesChange.on(listener);
}

} // namespace pegium
