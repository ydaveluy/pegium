#include <pegium/core/workspace/DefaultDocumentBuilder.hpp>

#include <algorithm>
#include <cassert>
#include <condition_variable>
#include <format>
#include <memory>
#include <ranges>
#include <stop_token>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

#include <pegium/core/services/CoreServices.hpp>
#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/services/SharedCoreServices.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/core/workspace/DocumentFactory.hpp>
#include <pegium/core/workspace/TextDocumentProvider.hpp>

namespace pegium::workspace {

namespace {

std::string_view document_state_name(DocumentState state) {
  switch (state) {
  case DocumentState::Changed:
    return "Changed";
  case DocumentState::Parsed:
    return "Parsed";
  case DocumentState::IndexedContent:
    return "IndexedContent";
  case DocumentState::ComputedScopes:
    return "ComputedScopes";
  case DocumentState::Linked:
    return "Linked";
  case DocumentState::IndexedReferences:
    return "IndexedReferences";
  case DocumentState::Validated:
    return "Validated";
  }
  return "Unknown";
}

std::size_t listener_index(DocumentState state) {
  return static_cast<std::size_t>(state);
}

DocumentId ensure_document_id(Documents &documents, Document &document) {
  if (document.id != InvalidDocumentId) {
    return document.id;
  }
  assert(!document.uri.empty());
  document.id = documents.getOrCreateDocumentId(document.uri);
  return document.id;
}

} // namespace

DefaultDocumentBuilder::DefaultDocumentBuilder(
    const pegium::SharedCoreServices &sharedServices)
    : pegium::DefaultSharedCoreService(sharedServices) {
  assert(sharedServices.workspace.documents != nullptr);
  assert(sharedServices.workspace.documentFactory != nullptr);
  assert(sharedServices.workspace.indexManager != nullptr);
  assert(sharedServices.serviceRegistry != nullptr);

  validation::ValidationOptions validationOptions;
  validationOptions.categories = {
      std::string(validation::kBuiltInValidationCategory),
      std::string(validation::kFastValidationCategory)};
  _updateBuildOptions.validation = std::move(validationOptions);
}

BuildOptions &DefaultDocumentBuilder::updateBuildOptions() noexcept {
  return _updateBuildOptions;
}

const BuildOptions &
DefaultDocumentBuilder::updateBuildOptions() const noexcept {
  return _updateBuildOptions;
}

void DefaultDocumentBuilder::build(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options, utils::CancellationToken cancelToken,
    const std::function<void()> &downgradeLock) const {
  auto &documentStore = *shared.workspace.documents;
  for (const auto &document : documents) {
    const auto documentId = ensure_document_id(documentStore, *document);
    if (document->state == DocumentState::Validated) {
      if (const auto *enabled = std::get_if<bool>(&options.validation);
          enabled != nullptr && *enabled) {
        resetToState(*document, DocumentState::IndexedReferences);
      } else if (std::holds_alternative<validation::ValidationOptions>(
                     options.validation)) {
        auto categories = findMissingValidationCategories(*document, options);
        if (!categories.empty()) {
          DocumentBuildState nextState;
          nextState.completed = false;
          nextState.options.validation = validation::ValidationOptions{
              .categories = std::move(categories),
          };
          {
            std::scoped_lock lock(_stateMutex);
            if (const auto state = _buildStateByDocumentId.find(documentId);
                state != _buildStateByDocumentId.end()) {
              nextState.result = state->second.result;
            }
            _buildStateByDocumentId.insert_or_assign(documentId,
                                                     std::move(nextState));
          }
          document->state = DocumentState::IndexedReferences;
        }
      }
    } else {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
  }
  {
    std::scoped_lock lock(_stateMutex);
    _currentState = DocumentState::Changed;
  }

  std::vector<DocumentId> changedDocumentIds;
  changedDocumentIds.reserve(documents.size());
  for (const auto &document : documents) {
    changedDocumentIds.push_back(ensure_document_id(documentStore, *document));
  }

  emitUpdate(changedDocumentIds, {});
  buildDocuments(documents, options, cancelToken, downgradeLock);
}

void DefaultDocumentBuilder::update(
    std::span<const DocumentId> changedDocumentIds,
    std::span<const DocumentId> deletedDocumentIds,
    utils::CancellationToken cancelToken,
    const std::function<void()> &downgradeLock) const {
  auto &documentStore = *shared.workspace.documents;
  utils::throw_if_cancelled(cancelToken);
  {
    std::scoped_lock lock(_stateMutex);
    _currentState = DocumentState::Changed;
  }

  std::unordered_set<DocumentId> deletedDocumentIdSet;
  std::unordered_set<DocumentId> changedDocumentIdSet;
  std::vector<DocumentId> orderedDeletedDocumentIds;
  std::vector<DocumentId> orderedChangedDocumentIds;

  for (const auto documentId : deletedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    if (documentId != InvalidDocumentId &&
        deletedDocumentIdSet.insert(documentId).second) {
      orderedDeletedDocumentIds.push_back(documentId);
    }
  }

  for (const auto documentId : changedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    if (documentId != InvalidDocumentId &&
        !deletedDocumentIdSet.contains(documentId) &&
        changedDocumentIdSet.insert(documentId).second) {
      orderedChangedDocumentIds.push_back(documentId);
    }
  }

  for (const auto documentId : orderedDeletedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    if (auto deletedDocument = documentStore.deleteDocument(documentId);
        deletedDocument != nullptr) {
      deletedDocument->state = DocumentState::Changed;
    }
    cleanUpDeleted(documentId);
  }

  for (const auto documentId : orderedChangedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    auto changedDocument = documentStore.getDocument(documentId);
    if (changedDocument == nullptr) {
      const auto uri = documentStore.getDocumentUri(documentId);
      if (uri.empty()) {
        continue;
      }
      try {
        changedDocument = documentStore.getOrCreateDocument(uri, cancelToken);
      } catch (const utils::OperationCancelled &) {
        throw;
      } catch (...) {
        // A changed document that cannot be materialized (e.g. unreadable on a
        // file-watch change) must not abort the rebuild of every other edited
        // document; skip it — a later change event will retry.
        continue;
      }
    }
    resetToState(*changedDocument, DocumentState::Changed);
  }

  std::unordered_set<DocumentId> allChangedDocumentIds;
  allChangedDocumentIds.reserve(orderedChangedDocumentIds.size() +
                                orderedDeletedDocumentIds.size());
  allChangedDocumentIds.insert(orderedChangedDocumentIds.begin(),
                               orderedChangedDocumentIds.end());
  allChangedDocumentIds.insert(orderedDeletedDocumentIds.begin(),
                               orderedDeletedDocumentIds.end());

  for (const auto &document : documentStore.all()) {
    utils::throw_if_cancelled(cancelToken);
    if (!allChangedDocumentIds.contains(document->id) &&
        shouldRelink(*document, allChangedDocumentIds)) {
      resetToState(*document, DocumentState::ComputedScopes);
    }
  }

  emitUpdate(orderedChangedDocumentIds, orderedDeletedDocumentIds);
  utils::throw_if_cancelled(cancelToken);

  auto allDocuments = documentStore.all();
  std::vector<std::shared_ptr<Document>> documentsToBuild;
  documentsToBuild.reserve(allDocuments.size());
  for (const auto &document : allDocuments) {
    bool completed = false;
    {
      std::scoped_lock lock(_stateMutex);
      if (const auto state = _buildStateByDocumentId.find(document->id);
          state != _buildStateByDocumentId.end()) {
        completed = state->second.completed;
      }
    }
    if (document->state < DocumentState::Validated || !completed ||
        resultsAreIncomplete(*document, _updateBuildOptions)) {
      documentsToBuild.push_back(document);
    }
  }
  documentsToBuild = sortDocuments(std::move(documentsToBuild));

  buildDocuments(documentsToBuild, _updateBuildOptions, cancelToken,
                 downgradeLock);
}

BuildOptions
DefaultDocumentBuilder::getBuildOptions(const Document &document) const {
  std::scoped_lock lock(_stateMutex);
  if (const auto state = _buildStateByDocumentId.find(document.id);
      state != _buildStateByDocumentId.end()) {
    return state->second.options;
  }
  return {};
}

bool DefaultDocumentBuilder::shouldLink(const Document &document) const {
  return getBuildOptions(document).eagerLinking.value_or(true);
}

bool DefaultDocumentBuilder::shouldValidate(const Document &document) const {
  const auto &validation = getBuildOptions(document).validation;
  if (const auto *enabled = std::get_if<bool>(&validation)) {
    return *enabled;
  }
  return std::holds_alternative<validation::ValidationOptions>(validation);
}

std::vector<std::string>
DefaultDocumentBuilder::findMissingValidationCategories(
    const Document &document, const BuildOptions &options) const {
  const auto &services =
      shared.serviceRegistry->getServices(document.uri);
  const auto allCategories =
      services.validation.validationRegistry->getAllValidationCategories();

  DocumentBuildState stateCopy;
  const DocumentBuildState *state = nullptr;
  {
    std::scoped_lock lock(_stateMutex);
    if (const auto it = _buildStateByDocumentId.find(document.id);
        it != _buildStateByDocumentId.end()) {
      stateCopy = it->second;
      state = std::addressof(stateCopy);
    }
  }

  std::unordered_set<std::string> executedCategories;
  if (state != nullptr && state->result.has_value() &&
      !state->result->validationChecks.empty()) {
    executedCategories.insert(state->result->validationChecks.begin(),
                              state->result->validationChecks.end());
  } else if (state != nullptr && state->completed) {
    executedCategories.insert(allCategories.begin(), allCategories.end());
  }

  std::vector<std::string> requestedCategories;
  if (std::holds_alternative<std::monostate>(options.validation)) {
    requestedCategories = {};
  } else if (const auto *enabled = std::get_if<bool>(&options.validation);
             enabled != nullptr) {
    requestedCategories = *enabled ? allCategories : std::vector<std::string>{};
  } else {
    const auto &validationOptions =
        std::get<validation::ValidationOptions>(options.validation);
    requestedCategories = validationOptions.categories.empty()
                              ? allCategories
                              : validationOptions.categories;
  }

  std::vector<std::string> missingCategories;
  missingCategories.reserve(requestedCategories.size());
  for (const auto &category : requestedCategories) {
    if (!executedCategories.contains(category)) {
      missingCategories.push_back(category);
    }
  }
  return missingCategories;
}

bool DefaultDocumentBuilder::resultsAreIncomplete(
    const Document &document, const BuildOptions &options) const {
  return !findMissingValidationCategories(document, options).empty();
}

bool DefaultDocumentBuilder::shouldRelink(
    const Document &document,
    const std::unordered_set<DocumentId> &changedDocumentIds) const {
  if (std::ranges::any_of(document.parseResult.references,
                          [](const ReferenceHandle &handle) {
                            return handle.getConst()->hasError();
                          })) {
    return true;
  }
  return shared.workspace.indexManager->isAffected(
      document, changedDocumentIds);
}

bool DefaultDocumentBuilder::hasTextDocument(
    const std::shared_ptr<Document> &document) const {
  const auto *textDocumentProvider =
      shared.workspace.textDocuments.get();
  assert(document != nullptr);
  assert(!document->uri.empty());
  return textDocumentProvider != nullptr &&
         textDocumentProvider->getNormalized(document->uri) != nullptr;
}

void DefaultDocumentBuilder::emitUpdate(
    std::span<const DocumentId> changedDocumentIds,
    std::span<const DocumentId> deletedDocumentIds) const {
  const auto listeners = snapshotListeners(_updateListeners);
  for (const auto &entry : listeners) {
    try {
      entry.listener(changedDocumentIds, deletedDocumentIds);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (...) {
      // An update listener (e.g. a cache invalidation) must not abort the
      // update or escape into the caller; isolate its failure.
    }
  }
}

std::vector<std::shared_ptr<Document>> DefaultDocumentBuilder::sortDocuments(
    std::vector<std::shared_ptr<Document>> documents) const {
  // Move documents that have an open text document to the front. Unstable —
  // the relative order within each group is not preserved, exactly as the
  // previous hand-rolled two-pointer partition did.
  std::partition(documents.begin(), documents.end(),
                 [this](const std::shared_ptr<Document> &document) {
                   return hasTextDocument(document);
                 });
  return documents;
}

void DefaultDocumentBuilder::prepareBuild(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options) const {
  auto &documentStore = *shared.workspace.documents;
  std::scoped_lock lock(_stateMutex);
  for (const auto &document : documents) {
    const auto documentId = ensure_document_id(documentStore, *document);
    const auto state = _buildStateByDocumentId.find(documentId);
    if (state == _buildStateByDocumentId.end() || state->second.completed) {
      DocumentBuildState nextState;
      nextState.completed = false;
      nextState.options = options;
      if (state != _buildStateByDocumentId.end()) {
        nextState.result = state->second.result;
      }
      _buildStateByDocumentId.insert_or_assign(documentId,
                                               std::move(nextState));
    }
  }
}

void DefaultDocumentBuilder::notifyBuildPhase(
    std::span<const std::shared_ptr<Document>> documents,
    DocumentState targetState, utils::CancellationToken cancelToken) const {
  if (documents.empty()) {
    return;
  }

  const auto listeners =
      snapshotListeners(_buildPhaseListeners[listener_index(targetState)]);
  for (const auto &entry : listeners) {
    utils::throw_if_cancelled(cancelToken);
    try {
      entry.listener(documents, cancelToken);
    } catch (const utils::OperationCancelled &) {
      throw;
    } catch (...) {
      // A build-phase listener (e.g. a cache invalidation) must not abort the
      // build or escape into build()/update(); isolate its failure.
    }
  }
}

void DefaultDocumentBuilder::notifyDocumentPhase(
    const std::shared_ptr<Document> &document, DocumentState targetState,
    utils::CancellationToken cancelToken) const {
  assert(document != nullptr);
  const auto listeners =
      snapshotListeners(_documentPhaseListeners[listener_index(targetState)]);

  std::exception_ptr cancellationError;
  std::exception_ptr listenerError;
  for (const auto &entry : listeners) {
    try {
      utils::throw_if_cancelled(cancelToken);
      entry.listener(document, cancelToken);
    } catch (const utils::OperationCancelled &) {
      if (cancellationError == nullptr) {
        cancellationError = std::current_exception();
      }
    } catch (...) {
      listenerError = std::current_exception();
      break;
    }
  }
  if (listenerError != nullptr) {
    std::rethrow_exception(listenerError);
  }
  if (cancellationError != nullptr) {
    std::rethrow_exception(cancellationError);
  }
}

void DefaultDocumentBuilder::publishWorkspaceState(
    DocumentState targetState) const {
  {
    std::scoped_lock lock(_stateMutex);
    _currentState = targetState;
    ++_publishedWorkspaceStates[listener_index(targetState)];
  }
  _stateCv.notify_all();
}

void DefaultDocumentBuilder::advance(const std::shared_ptr<Document> &document,
                                     DocumentState targetState,
                                     utils::CancellationToken /*cancelToken*/) const {
  // Only records the document's state; phase listeners are notified separately,
  // serially and in document order, once the phase has drained (runMergedPhase).
  document->state = targetState;
}

void DefaultDocumentBuilder::buildDocuments(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options, utils::CancellationToken cancelToken,
    const std::function<void()> &downgradeLock) const {
  prepareBuild(documents, options);

  std::vector<std::shared_ptr<Document>> documentsToBuild(documents.begin(),
                                                          documents.end());

  // Phase A: parse + index this document's exported content. Both are
  // per-document local; each document advances through Parsed then
  // IndexedContent on the same worker, notifying its phase listeners inline.
  runMergedPhase(
      documentsToBuild, DocumentState::IndexedContent,
      {DocumentState::Parsed, DocumentState::IndexedContent}, cancelToken,
      [this](const std::shared_ptr<Document> &document, DocumentState entry,
             const utils::CancellationToken &phaseToken) {
        if (entry < DocumentState::Parsed) {
          shared.workspace.documentFactory->update(*document, phaseToken);
          advance(document, DocumentState::Parsed, phaseToken);
        }
        if (entry < DocumentState::IndexedContent) {
          shared.workspace.indexManager->updateContent(*document, phaseToken);
          advance(document, DocumentState::IndexedContent, phaseToken);
        }
      });

  // Barrier between Phase A and Phase B is mandatory: linking resolves
  // cross-document references against the global content index, which is only
  // complete once every document has finished Phase A.

  // Phase B: compute local scopes for every document, then link and index this
  // document's references — but only for documents that should be linked.
  // Local-scope computation runs for all documents regardless of eager linking.
  runMergedPhase(
      documentsToBuild, DocumentState::IndexedReferences,
      {DocumentState::ComputedScopes, DocumentState::Linked,
       DocumentState::IndexedReferences},
      cancelToken,
      [this](const std::shared_ptr<Document> &document, DocumentState entry,
             const utils::CancellationToken &phaseToken) {
        const auto &services = shared.serviceRegistry->getServices(document->uri);
        if (entry < DocumentState::ComputedScopes) {
          document->localSymbols =
              services.references.scopeComputation->collectLocalSymbols(
                  *document, phaseToken);
          advance(document, DocumentState::ComputedScopes, phaseToken);
        }
        if (shouldLink(*document)) {
          if (entry < DocumentState::Linked) {
            services.references.linker->link(*document, phaseToken);
            advance(document, DocumentState::Linked, phaseToken);
          }
          if (entry < DocumentState::IndexedReferences) {
            shared.workspace.indexManager->updateReferences(*document,
                                                            phaseToken);
            advance(document, DocumentState::IndexedReferences, phaseToken);
          }
        }
      });

  std::vector<std::shared_ptr<Document>> toBeValidated;
  toBeValidated.reserve(documentsToBuild.size());
  for (const auto &document : documentsToBuild) {
    if (shouldValidate(*document)) {
      toBeValidated.push_back(document);
    } else {
      markAsCompleted(*document);
    }
  }

  // The document model is now fully linked. Release the exclusive lock (if the
  // caller supplied a downgrade) so reads can proceed while validation runs —
  // validation only reads the linked model and writes diagnostics.
  if (downgradeLock) {
    downgradeLock();
  }

  // Phase C: validate. Reads the linked model and writes only diagnostics.
  runMergedPhase(
      toBeValidated, DocumentState::Validated, {DocumentState::Validated},
      cancelToken,
      [this](const std::shared_ptr<Document> &document, DocumentState entry,
             const utils::CancellationToken &phaseToken) {
        if (entry < DocumentState::Validated) {
          validate(*document, phaseToken);
          markAsCompleted(*document);
          advance(document, DocumentState::Validated, phaseToken);
        }
      });
}

void DefaultDocumentBuilder::markAsCompleted(const Document &document) const {
  std::scoped_lock lock(_stateMutex);
  if (const auto state = _buildStateByDocumentId.find(document.id);
      state != _buildStateByDocumentId.end()) {
    state->second.completed = true;
  }
}

void DefaultDocumentBuilder::validate(
    Document &document, utils::CancellationToken cancelToken) const {
  const auto validator =
      shared.serviceRegistry
          ->getServices(document.uri)
          .validation.documentValidator.get();
  const auto options = getBuildOptions(document);
  validation::ValidationOptions validationOptions;
  if (const auto *configuredValidationOptions =
          std::get_if<validation::ValidationOptions>(&options.validation);
      configuredValidationOptions != nullptr) {
    validationOptions = *configuredValidationOptions;
  }
  validationOptions.categories =
      findMissingValidationCategories(document, options);
  auto diagnostics =
      validator->validateDocument(document, validationOptions, cancelToken);
  if (!document.diagnostics.empty()) {
    document.diagnostics.insert(
        document.diagnostics.end(),
        std::make_move_iterator(diagnostics.begin()),
        std::make_move_iterator(diagnostics.end()));
  } else {
    document.diagnostics = std::move(diagnostics);
  }

  std::scoped_lock lock(_stateMutex);
  auto &state = _buildStateByDocumentId[document.id];
  // Keep the cumulative validation-check history across incremental builds:
  // findMissingValidationCategories relies on it to avoid re-running (and
  // re-appending diagnostics for) categories already validated. Resetting it
  // here would make a later build re-validate earlier categories.
  if (!state.result.has_value()) {
    state.result.emplace();
  }
  auto &validationChecks = state.result->validationChecks;
  for (const auto &category : validationOptions.categories) {
    if (std::ranges::find(validationChecks, category) ==
        validationChecks.end()) {
      validationChecks.push_back(category);
    }
  }
}

void DefaultDocumentBuilder::waitUntil(
    DocumentState state, utils::CancellationToken cancelToken) const {
  awaitBuilderState(state, cancelToken);
}

DocumentId DefaultDocumentBuilder::waitUntil(
    DocumentState state, DocumentId documentId,
    utils::CancellationToken cancelToken) const {
  return awaitDocumentState(state, documentId, cancelToken);
}

void DefaultDocumentBuilder::awaitBuilderState(
    DocumentState state, utils::CancellationToken cancelToken) const {
  const auto index = listener_index(state);
  std::unique_lock lock(_stateMutex);
  if (_currentState >= state) {
    return;
  }
  utils::throw_if_cancelled(cancelToken);

  const auto publishedState = _publishedWorkspaceStates[index];
  const auto onCancelled = [this]() { _stateCv.notify_all(); };
  std::stop_callback cancelCallback(cancelToken, onCancelled);
  (void)cancelCallback;

  _stateCv.wait(lock, [this, state, index, publishedState, &cancelToken]() {
    return _currentState >= state ||
           _publishedWorkspaceStates[index] != publishedState ||
           cancelToken.stop_requested();
  });
  utils::throw_if_cancelled(cancelToken);
}

DocumentId DefaultDocumentBuilder::awaitDocumentState(
    DocumentState state, DocumentId documentId,
    utils::CancellationToken cancelToken) const {
  if (documentId == InvalidDocumentId) {
    throw utils::DocumentBuilderError("No document id provided.");
  }

  const auto make_failure = [&](DocumentState documentState,
                                DocumentState workspaceState) {
    return std::make_exception_ptr(utils::DocumentBuilderError(std::format(
        "Document state of #{} is {}, requiring {}, but workspace state "
        "is already {}.",
        documentId, document_state_name(documentState), document_state_name(state),
        document_state_name(workspaceState))));
  };
  const auto check_now =
      [&](std::exception_ptr &error) {
    const auto document =
        shared.workspace.documents->getDocument(documentId);
    if (document == nullptr) {
      error = std::make_exception_ptr(utils::DocumentBuilderError(
          std::format("No document found for id: {}", documentId)));
      return true;
    }
    if (document->state >= state) {
      return true;
    }
    std::scoped_lock lock(_stateMutex);
    if (_currentState >= state && state > document->state) {
      error = make_failure(document->state, _currentState);
      return true;
    }
    return false;
  };

  if (std::exception_ptr initialError; check_now(initialError)) {
    if (initialError != nullptr) {
      std::rethrow_exception(initialError);
    }
    return documentId;
  }
  utils::throw_if_cancelled(cancelToken);

  // The wait state is shared with the onDocumentPhase listener registered below.
  // That listener can be invoked by a build thread off a *snapshot copy* of the
  // listener list taken before this function returns: notifyDocumentPhase
  // snapshots the listeners under the lock, then invokes them outside it, so
  // unregistering the listener (when `listener` is destroyed on return) does not
  // synchronize with an invocation already in flight. Hold the wait state on the
  // heap and capture it by value into every continuation, so a late listener
  // invocation operates on still-alive state instead of a destroyed stack frame.
  struct WaitState {
    std::mutex mutex;
    std::condition_variable cv;
    bool completed = false;
    std::exception_ptr error;
  };
  const auto waitState = std::make_shared<WaitState>();

  const auto finish = [waitState](std::exception_ptr error = nullptr) {
    std::scoped_lock lock(waitState->mutex);
    if (waitState->completed) {
      return;
    }
    waitState->completed = true;
    waitState->error = std::move(error);
    waitState->cv.notify_all();
  };

  auto listener = onDocumentPhase(
      state, [documentId, finish](const std::shared_ptr<Document> &document,
                                  utils::CancellationToken) {
        if (document->id == documentId) {
          finish();
        }
      });
  (void)listener;
  std::stop_callback cancelCallback(cancelToken, [finish]() {
    finish(std::make_exception_ptr(utils::OperationCancelled()));
  });
  (void)cancelCallback;

  if (std::exception_ptr recheckError; check_now(recheckError)) {
    finish(recheckError);
  }

  std::unique_lock lock(waitState->mutex);
  waitState->cv.wait(lock, [&waitState] { return waitState->completed; });
  if (waitState->error != nullptr) {
    std::rethrow_exception(waitState->error);
  }
  return documentId;
}

void DefaultDocumentBuilder::cleanUpDeleted(DocumentId documentId) const {
  {
    std::scoped_lock lock(_stateMutex);
    _buildStateByDocumentId.erase(documentId);
  }
  shared.workspace.indexManager->remove(documentId);
}

utils::ScopedDisposable DefaultDocumentBuilder::onUpdate(
    std::function<void(std::span<const DocumentId> changedDocumentIds,
                       std::span<const DocumentId> deletedDocumentIds)>
        listener) const {
  return addListener(_updateListeners, std::move(listener));
}

utils::ScopedDisposable DefaultDocumentBuilder::onBuildPhase(
    DocumentState targetState,
    std::function<
        void(std::span<const std::shared_ptr<Document>> builtDocuments,
             utils::CancellationToken cancelToken)>
        listener) const {
  return addListener(_buildPhaseListeners[listener_index(targetState)],
                     std::move(listener));
}

utils::ScopedDisposable DefaultDocumentBuilder::onDocumentPhase(
    DocumentState targetState,
    std::function<void(const std::shared_ptr<Document> &,
                       utils::CancellationToken cancelToken)>
        listener) const {
  return addListener(_documentPhaseListeners[listener_index(targetState)],
                     std::move(listener));
}

void DefaultDocumentBuilder::resetToState(Document &document,
                                          DocumentState state) const {
  const auto documentId =
      ensure_document_id(*shared.workspace.documents, document);

  switch (state) {
  case DocumentState::Changed:
  case DocumentState::Parsed:
    shared.workspace.indexManager->removeContent(documentId);
    [[fallthrough]];
  case DocumentState::IndexedContent:
    document.localSymbols.clear();
    [[fallthrough]];
  case DocumentState::ComputedScopes:
    shared.serviceRegistry
        ->getServices(document.uri)
        .references.linker->unlink(document);
    [[fallthrough]];
  case DocumentState::Linked:
    shared.workspace.indexManager->removeReferences(documentId);
    [[fallthrough]];
  case DocumentState::IndexedReferences:
    document.diagnostics.clear();
    {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    [[fallthrough]];
  case DocumentState::Validated:
    break;
  }

  if (document.state > state) {
    document.state = state;
  }
}

} // namespace pegium::workspace
