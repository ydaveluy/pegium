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
    const BuildOptions &options, utils::CancellationToken cancelToken) const {
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
  buildDocuments(documents, options, cancelToken);
}

void DefaultDocumentBuilder::update(
    std::span<const DocumentId> changedDocumentIds,
    std::span<const DocumentId> deletedDocumentIds,
    utils::CancellationToken cancelToken) const {
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
      changedDocument = documentStore.getOrCreateDocument(uri, cancelToken);
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

  buildDocuments(documentsToBuild, _updateBuildOptions, cancelToken);
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
  if (std::ranges::any_of(document.references,
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
         textDocumentProvider->get(document->uri) != nullptr;
}

void DefaultDocumentBuilder::emitUpdate(
    std::span<const DocumentId> changedDocumentIds,
    std::span<const DocumentId> deletedDocumentIds) const {
  const auto listeners = snapshotListeners(_updateListeners);
  for (const auto &entry : listeners) {
    entry.listener(changedDocumentIds, deletedDocumentIds);
  }
}

std::vector<std::shared_ptr<Document>> DefaultDocumentBuilder::sortDocuments(
    std::vector<std::shared_ptr<Document>> documents) const {
  std::size_t left = 0;
  std::size_t right = documents.empty() ? 0 : documents.size() - 1;

  while (left < right) {
    while (left < documents.size() && hasTextDocument(documents[left])) {
      ++left;
    }
    while (right > 0 && !hasTextDocument(documents[right])) {
      --right;
    }
    if (left < right) {
      std::swap(documents[left], documents[right]);
    }
  }

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
    entry.listener(documents, cancelToken);
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

void DefaultDocumentBuilder::buildDocuments(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options, utils::CancellationToken cancelToken) const {
  prepareBuild(documents, options);

  std::vector<std::shared_ptr<Document>> documentsToBuild(documents.begin(),
                                                          documents.end());

  runCancelable(
      documentsToBuild, DocumentState::Parsed, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        shared.workspace.documentFactory->update(document,
                                                             phaseToken);
      });

  runCancelable(
      documentsToBuild, DocumentState::IndexedContent, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        shared.workspace.indexManager->updateContent(document,
                                                                 phaseToken);
      });

  runCancelable(
      documentsToBuild, DocumentState::ComputedScopes, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        const auto scopeComputation =
            shared.serviceRegistry
                ->getServices(document.uri)
                .references.scopeComputation.get();
        document.localSymbols =
            scopeComputation->collectLocalSymbols(document, phaseToken);
      });

  std::vector<std::shared_ptr<Document>> toBeLinked;
  toBeLinked.reserve(documentsToBuild.size());
  for (const auto &document : documentsToBuild) {
    if (shouldLink(*document)) {
      toBeLinked.push_back(document);
    }
  }

  runCancelable(
      toBeLinked, DocumentState::Linked, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        const auto linker =
            shared.serviceRegistry
                ->getServices(document.uri)
                .references.linker.get();
        linker->link(document, phaseToken);
      });

  runCancelable(
      toBeLinked, DocumentState::IndexedReferences, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        shared.workspace.indexManager->updateReferences(
            document, phaseToken);
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

  runCancelable(
      toBeValidated, DocumentState::Validated, cancelToken,
      [this](Document &document, const utils::CancellationToken &phaseToken) {
        validate(document, phaseToken);
        markAsCompleted(document);
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
  state.result.emplace();
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
      [&](std::exception_ptr &error) -> bool {
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

  std::exception_ptr initialError;
  if (check_now(initialError)) {
    if (initialError != nullptr) {
      std::rethrow_exception(initialError);
    }
    return documentId;
  }
  utils::throw_if_cancelled(cancelToken);

  std::mutex waitMutex;
  std::condition_variable waitCv;
  bool completed = false;
  std::exception_ptr waitError;

  const auto finish = [&](std::exception_ptr error = nullptr) {
    std::scoped_lock lock(waitMutex);
    if (completed) {
      return;
    }
    completed = true;
    waitError = std::move(error);
    waitCv.notify_all();
  };

  auto listener = onDocumentPhase(
      state, [documentId, &finish](const std::shared_ptr<Document> &document,
                                   utils::CancellationToken) {
        if (document->id == documentId) {
          finish();
        }
      });
  (void)listener;
  auto onCancelled = [&finish]() {
    finish(std::make_exception_ptr(utils::OperationCancelled()));
  };
  std::stop_callback cancelCallback(cancelToken, onCancelled);
  (void)cancelCallback;

  std::exception_ptr recheckError;
  if (check_now(recheckError)) {
    finish(recheckError);
  }

  std::unique_lock lock(waitMutex);
  waitCv.wait(lock, [&completed] { return completed; });
  if (waitError != nullptr) {
    std::rethrow_exception(waitError);
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
