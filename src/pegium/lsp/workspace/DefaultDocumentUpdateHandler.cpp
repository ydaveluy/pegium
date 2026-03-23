#include <pegium/lsp/workspace/DefaultDocumentUpdateHandler.hpp>

#include <cassert>
#include <future>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>

#include <lsp/messages.h>

#include <pegium/core/observability/ObservabilitySink.hpp>
#include <pegium/lsp/runtime/internal/RuntimeObservability.hpp>
#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/utils/UriUtils.hpp>

namespace pegium {

namespace {

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
    std::span<const workspace::DocumentId> changedDocumentIds,
    std::span<const workspace::DocumentId> deletedDocumentIds, std::string message) {
  observability::Observation observation{
      .severity = observability::ObservationSeverity::Error,
      .code = observability::ObservationCode::DocumentUpdateDispatchFailed,
      .message = std::move(message)};
  const auto documentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
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

DefaultDocumentUpdateHandler::~DefaultDocumentUpdateHandler() = default;

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

namespace {

bool is_redundant_text_snapshot(
    const workspace::Documents &documents,
    const workspace::TextDocument &textDocument) {
  if (const auto document = documents.getDocument(textDocument.uri());
      document != nullptr) {
    if (document->parseResult.cst != nullptr) {
      return document->parseResult.cst->getText() == textDocument.getText();
    }
    return document->textDocument().getText() == textDocument.getText();
  }
  return false;
}

} // namespace

void DefaultDocumentUpdateHandler::didChangeContent(
    const TextDocumentChangeEvent &event) {
  auto &documents = *shared.workspace.documents;
  if (is_redundant_text_snapshot(documents, *event.document)) {
    return;
  }
  fireDocumentUpdate({documents.getOrCreateDocumentId(event.document->uri())}, {});
}

void DefaultDocumentUpdateHandler::fireDocumentUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds) {
  auto future = std::async(
      std::launch::async,
      [this, changedDocumentIds = std::move(changedDocumentIds),
       deletedDocumentIds = std::move(deletedDocumentIds)]() mutable {
        try {
          // `ready()` only guarantees that startup documents were discovered
          // and materialized. The tail of the initial build may still be
          // running here and can be superseded by this newer workspace write.
          auto ready = shared.workspace.workspaceManager->ready();
          ready.get();
          auto writeFuture = shared.workspace.workspaceLock->write(
              [this, changedDocumentIds = std::move(changedDocumentIds),
               deletedDocumentIds = std::move(deletedDocumentIds)](
                  const utils::CancellationToken &cancelToken) mutable {
                applyDocumentUpdate(std::move(changedDocumentIds),
                                    std::move(deletedDocumentIds), cancelToken);
              });
          writeFuture.get();
        } catch (const utils::OperationCancelled &) {
        } catch (const std::exception &error) {
          publish_document_update_dispatch_failed(
              shared, changedDocumentIds, deletedDocumentIds,
              "Workspace initialization failed. Could not perform document "
              "update: " +
                  std::string(error.what()));
        } catch (...) {
          publish_document_update_dispatch_failed(
              shared, changedDocumentIds, deletedDocumentIds,
              "Workspace initialization failed. Could not perform document "
              "update.");
        }
      });
  observe_background_task(shared, "DocumentUpdateHandler.fireDocumentUpdate",
                          std::move(future));
}

void DefaultDocumentUpdateHandler::applyDocumentUpdate(
    std::vector<workspace::DocumentId> changedDocumentIds,
    std::vector<workspace::DocumentId> deletedDocumentIds,
    const utils::CancellationToken &cancelToken) {
  utils::throw_if_cancelled(cancelToken);

  auto &documentBuilder = *shared.workspace.documentBuilder;

  const auto documentId =
      select_update_document_id(changedDocumentIds, deletedDocumentIds);
  auto originalOptions = documentBuilder.updateBuildOptions();
  auto effectiveOptions = originalOptions;
  const bool hasValidationOverride =
      merge_validation_options_for_document(shared, documentId, effectiveOptions);
  if (hasValidationOverride) {
    documentBuilder.updateBuildOptions() = effectiveOptions;
  }

  try {
    documentBuilder.update(changedDocumentIds, deletedDocumentIds, cancelToken);
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

  for (const auto &change : params.changes) {
    const auto uri = utils::normalize_uri(change.uri.toString());
    if (uri.empty()) {
      continue;
    }

    if (change.type == ::lsp::FileChangeType::Deleted) {
      if (const auto documentId = documents.getDocumentId(uri);
          documentId != workspace::InvalidDocumentId &&
          seenDeleted.insert(documentId).second) {
        deletedDocumentIds.push_back(documentId);
      }
      continue;
    }

    if (const auto documentId = documents.getOrCreateDocumentId(uri);
        !seenDeleted.contains(documentId) &&
        seenChanged.insert(documentId).second) {
      changedDocumentIds.push_back(documentId);
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
