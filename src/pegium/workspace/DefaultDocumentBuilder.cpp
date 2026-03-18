#include <pegium/workspace/DefaultDocumentBuilder.hpp>

#include <algorithm>
#include <chrono>
#include <functional>
#include <format>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/grammar/Create.hpp>
#include <pegium/grammar/Group.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/grammar/Nest.hpp>
#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/grammar/UnorderedGroup.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <pegium/services/CoreServices.hpp>
#include <pegium/services/ServiceRegistry.hpp>
#include <pegium/services/SharedCoreServices.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/utils/Cancellation.hpp>
#include <pegium/utils/Stream.hpp>
#include <pegium/validation/ValidationRegistry.hpp>
#include <pegium/workspace/ParseDiagnosticsExtractor.hpp>
#include <pegium/workspace/Symbol.hpp>

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

struct BuildEntry {
  std::shared_ptr<Document> document;
  const services::CoreServices *services = nullptr;
};

const BuildOptions *get_build_options(
    const std::unordered_map<DocumentId, DocumentBuildState> &buildStateByDocumentId,
    DocumentId documentId) {
  if (documentId == InvalidDocumentId) {
    return nullptr;
  }
  const auto it = buildStateByDocumentId.find(documentId);
  return it == buildStateByDocumentId.end() ? nullptr
                                            : std::addressof(it->second.options);
}

bool contains_all_categories(const std::vector<std::string> &available,
                             const std::vector<std::string> &requested) {
  return std::ranges::all_of(requested, [&available](const std::string &category) {
    return std::ranges::find(available, category) != available.end();
  });
}

bool validation_results_incomplete(const DocumentBuildState *state,
                                   const BuildOptions &options) {
  if (!options.validation.enabled) {
    return false;
  }
  if (state == nullptr || !state->completed) {
    return true;
  }
  const auto &requestedCategories = options.validation.categories;
  const auto &validatedCategories =
      !state->result.has_value() ? std::vector<std::string>{}
                                 : state->result->validationCategories;
  if (requestedCategories.empty()) {
    return validatedCategories.empty();
  }
  if (validatedCategories.empty()) {
    return true;
  }
  return !contains_all_categories(validatedCategories, requestedCategories);
}

std::vector<std::string> all_validation_categories(
    const services::CoreServices &services) {
  if (services.validation.validationRegistry == nullptr) {
    return {};
  }
  return services.validation.validationRegistry->getAllValidationCategories();
}

std::vector<std::string> requested_validation_categories(
    const services::CoreServices &services, const BuildOptions &options) {
  if (!options.validation.enabled) {
    return {};
  }
  if (!options.validation.categories.empty()) {
    return options.validation.categories;
  }
  return all_validation_categories(services);
}

std::vector<std::string> missing_validation_categories(
    const services::CoreServices &services, const DocumentBuildState *state,
    const BuildOptions &options) {
  const auto requested =
      requested_validation_categories(services, options);
  if (requested.empty()) {
    return {};
  }

  const auto &executed =
      state != nullptr && state->result.has_value()
          ? state->result->validationCategories
          : std::vector<std::string>{};
  if (executed.empty()) {
    return requested;
  }

  std::vector<std::string> missing;
  missing.reserve(requested.size());
  for (const auto &category : requested) {
    if (std::ranges::find(executed, category) == executed.end()) {
      missing.push_back(category);
    }
  }
  return missing;
}

void append_diagnostics(std::vector<services::Diagnostic> &target,
                        std::vector<services::Diagnostic> source) {
  target.insert(target.end(), std::make_move_iterator(source.begin()),
                std::make_move_iterator(source.end()));
}

bool has_unresolved_references(const Document &document) {
  return std::ranges::any_of(
      document.referenceDescriptions, [](const ReferenceDescription &reference) {
        return !reference.isResolved();
  });
}

bool has_open_text_document(const services::SharedCoreServices &sharedServices,
                            const std::shared_ptr<Document> &document) {
  return document != nullptr &&
         sharedServices.workspace.textDocuments != nullptr &&
         !document->uri.empty() &&
         sharedServices.workspace.textDocuments->get(document->uri) != nullptr;
}

void sort_documents_for_update(
    const services::SharedCoreServices &sharedServices,
    std::vector<std::shared_ptr<Document>> &documents) {
  std::ranges::stable_sort(
      documents, [&sharedServices](const std::shared_ptr<Document> &left,
                                   const std::shared_ptr<Document> &right) {
        if (!left || !right) {
          return static_cast<bool>(left) > static_cast<bool>(right);
        }

        const auto leftOpen = has_open_text_document(sharedServices, left);
        const auto rightOpen = has_open_text_document(sharedServices, right);
        if (leftOpen != rightOpen) {
          return leftOpen;
        }

        return left->uri < right->uri;
      });
}

std::string resolve_language_id(const services::SharedCoreServices &sharedServices,
                                std::string_view uri) {
  if (sharedServices.workspace.textDocuments != nullptr) {
    if (auto textDocument = sharedServices.workspace.textDocuments->get(uri);
        textDocument != nullptr && !textDocument->languageId.empty()) {
      return textDocument->languageId;
    }
  }
  if (sharedServices.serviceRegistry == nullptr) {
    return {};
  }
  const auto *services = sharedServices.serviceRegistry->getServices(uri);
  return services == nullptr ? std::string{} : services->languageId;
}

DocumentId ensure_document_id(services::SharedCoreServices &sharedServices,
                              Document &document) {
  if (document.id != InvalidDocumentId) {
    return document.id;
  }
  if (document.uri.empty() || sharedServices.workspace.documents == nullptr) {
    return InvalidDocumentId;
  }
  document.id =
      sharedServices.workspace.documents->getOrCreateDocumentId(document.uri);
  return document.id;
}

BuildOptions build_options_for_document(
    const std::unordered_map<DocumentId, DocumentBuildState> &buildStateByDocumentId,
    const Document &document, const BuildOptions &defaultOptions) {
  if (const auto *options = get_build_options(buildStateByDocumentId, document.id)) {
    return *options;
  }
  return defaultOptions;
}

template <typename PhaseFn, typename DocumentPhaseFn>
bool run_phase(std::vector<BuildEntry> &entries, DocumentState phaseState,
               std::vector<std::shared_ptr<Document>> &phaseDocuments,
               const utils::CancellationToken &cancelToken, PhaseFn &&phase,
               DocumentPhaseFn &&onDocumentPhase) {
  phaseDocuments.clear();
  for (auto &entry : entries) {
    if (!entry.document) {
      continue;
    }
    if (entry.document->state < phaseState) {
      utils::throw_if_cancelled(cancelToken);
      phase(entry);
      entry.document->state = phaseState;
      onDocumentPhase(entry.document);
    }
    if (entry.document->state == phaseState) {
      phaseDocuments.push_back(entry.document);
    }
  }
  return true;
}

template <typename PhaseFn, typename DocumentPhaseFn>
bool run_phase_parallel_or_inline(
    execution::TaskScheduler *scheduler, std::vector<BuildEntry> &entries,
    DocumentState phaseState,
    std::vector<std::shared_ptr<Document>> &phaseDocuments,
    const utils::CancellationToken &cancelToken, PhaseFn &&phase,
    DocumentPhaseFn &&onDocumentPhase) {
  if (scheduler == nullptr) {
    return run_phase(entries, phaseState, phaseDocuments, cancelToken,
                     [&phase, &cancelToken](BuildEntry &entry) {
                       phase(entry, cancelToken);
                     },
                     std::forward<DocumentPhaseFn>(onDocumentPhase));
  }

  std::vector<std::size_t> pendingEntries;
  pendingEntries.reserve(entries.size());
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (entries[index].document != nullptr &&
        entries[index].document->state < phaseState) {
      pendingEntries.push_back(index);
    }
  }

  if (!pendingEntries.empty()) {
    if (pendingEntries.size() == 1) {
      const auto index = pendingEntries.front();
      utils::throw_if_cancelled(cancelToken);
      phase(entries[index], cancelToken);
      entries[index].document->state = phaseState;
      onDocumentPhase(entries[index].document);
    } else {
      scheduler->parallelFor(
          cancelToken, pendingEntries,
          [&entries, &phase](execution::TaskScheduler::Scope &scope,
                             std::size_t index) {
            phase(entries[index], scope.cancellationToken());
          });

      for (const auto index : pendingEntries) {
        entries[index].document->state = phaseState;
        onDocumentPhase(entries[index].document);
      }
    }
  }

  phaseDocuments.clear();
  for (const auto &entry : entries) {
    if (entry.document != nullptr && entry.document->state == phaseState) {
      phaseDocuments.push_back(entry.document);
    }
  }
  return true;
}

} // namespace

DefaultDocumentBuilder::DefaultDocumentBuilder(
    services::SharedCoreServices &sharedServices)
    : services::DefaultSharedCoreService(sharedServices) {
  _updateBuildOptions.validation.enabled = true;
  _updateBuildOptions.validation.categories = {
      std::string(validation::kBuiltInValidationCategory),
      std::string(validation::kFastValidationCategory)};
}

BuildOptions &DefaultDocumentBuilder::updateBuildOptions() noexcept {
  return _updateBuildOptions;
}

const BuildOptions &DefaultDocumentBuilder::updateBuildOptions() const noexcept {
  return _updateBuildOptions;
}

bool DefaultDocumentBuilder::build(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options, utils::CancellationToken cancelToken) const {
  const auto *serviceRegistry = sharedCoreServices.serviceRegistry.get();
  {
    std::scoped_lock lock(_stateMutex);
    for (const auto &document : documents) {
      if (!document || document->uri.empty()) {
        continue;
      }
      const auto documentId = ensure_document_id(sharedCoreServices, *document);
      if (documentId == InvalidDocumentId) {
        continue;
      }

      if (document->state == DocumentState::Validated) {
        if (!options.validation.enabled) {
          continue;
        }

        const auto *services =
            serviceRegistry == nullptr
                ? nullptr
                : serviceRegistry->getServicesByLanguageId(document->languageId);
        const auto it = _buildStateByDocumentId.find(documentId);
        const auto *state =
            it != _buildStateByDocumentId.end() ? std::addressof(it->second)
                                                : nullptr;
        const auto categories =
            services == nullptr
                ? std::vector<std::string>{}
                : missing_validation_categories(*services, state, options);

        if (!categories.empty()) {
          auto &buildState = _buildStateByDocumentId[documentId];
          buildState.completed = false;
          buildState.options = options;
          buildState.options.validation.categories = std::move(categories);
          document->state = DocumentState::IndexedReferences;
        }
      } else {
        _buildStateByDocumentId.erase(documentId);
      }
    }

    _currentState = DocumentState::Changed;
  }
  _stateCv.notify_all();
  return buildInternal(documents, options, cancelToken, true);
}

bool DefaultDocumentBuilder::buildInternal(
    std::span<const std::shared_ptr<Document>> documents,
    const BuildOptions &options, utils::CancellationToken cancelToken,
    bool emitUpdateEvent) const {
  utils::throw_if_cancelled(cancelToken);
  if (emitUpdateEvent) {
    std::vector<DocumentId> changedDocumentIds;
    std::unordered_set<DocumentId> seen;
    changedDocumentIds.reserve(documents.size());

    for (const auto &document : documents) {
      if (!document) {
        continue;
      }
      const auto documentId =
          ensure_document_id(sharedCoreServices, *document);
      if (documentId != InvalidDocumentId && seen.insert(documentId).second) {
        changedDocumentIds.push_back(documentId);
      }
    }

    if (!changedDocumentIds.empty()) {
      DocumentUpdateEvent updateEvent;
      updateEvent.changedDocumentIds = std::move(changedDocumentIds);
      _onUpdate.emit(updateEvent);
    }
  }

  std::vector<BuildEntry> entries;
  entries.reserve(documents.size());

  bool allServicesResolved = true;
  const auto *serviceRegistry = sharedCoreServices.serviceRegistry.get();
  const auto *documentFactory = sharedCoreServices.workspace.documentFactory.get();
  if (serviceRegistry == nullptr || documentFactory == nullptr) {
    return false;
  }
  for (const auto &document : documents) {
    utils::throw_if_cancelled(cancelToken);
    if (!document) {
      allServicesResolved = false;
      continue;
    }

    auto services = serviceRegistry->getServicesByLanguageId(document->languageId);
    if (!services || !services->isComplete()) {
      allServicesResolved = false;
      continue;
    }

    entries.push_back({document, services});
  }

  if (entries.empty()) {
    return allServicesResolved;
  }

  {
    std::scoped_lock lock(_stateMutex);
    for (const auto &entry : entries) {
      if (!entry.document) {
        continue;
      }
      const auto documentId =
          ensure_document_id(sharedCoreServices, *entry.document);
      if (documentId == InvalidDocumentId) {
        continue;
      }
      const auto it = _buildStateByDocumentId.find(documentId);
      if (it == _buildStateByDocumentId.end() || it->second.completed) {
        DocumentBuildState nextState;
        nextState.completed = false;
        nextState.options = options;
        if (it != _buildStateByDocumentId.end()) {
          nextState.result = it->second.result;
          it->second = std::move(nextState);
        } else {
          _buildStateByDocumentId.emplace(documentId, std::move(nextState));
        }
      }
    }
  }

  auto emit_phase =
      [this](DocumentState targetState,
             const std::vector<std::shared_ptr<Document>> &builtDocuments) {
    if (builtDocuments.empty()) {
      return;
    }
    DocumentBuildPhaseEvent phaseEvent;
    phaseEvent.targetState = targetState;
    phaseEvent.builtDocuments = builtDocuments;
    _onBuildPhase.emit(phaseEvent);
    {
      std::scoped_lock lock(_stateMutex);
      _currentState = targetState;
    }
    _stateCv.notify_all();
  };

  auto emit_document_phase = [this](DocumentState targetState,
                                    const std::shared_ptr<Document> &document) {
    if (!document) {
      return;
    }
    DocumentPhaseEvent phaseEvent;
    phaseEvent.targetState = targetState;
    phaseEvent.builtDocument = document;
    _onDocumentPhase.emit(phaseEvent);
    _stateCv.notify_all();
  };

  std::vector<std::shared_ptr<Document>> phaseDocuments;

  auto *taskScheduler = sharedCoreServices.execution.taskScheduler.get();

  if (!run_phase_parallel_or_inline(
          taskScheduler, entries, DocumentState::Parsed, phaseDocuments,
          cancelToken,
          [documentFactory](BuildEntry &entry,
                            const utils::CancellationToken &phaseToken) {
            (void)documentFactory->update(*entry.document, phaseToken);
          },
          [&emit_document_phase](const std::shared_ptr<Document> &document) {
            emit_document_phase(DocumentState::Parsed, document);
          })) {
    return false;
  }
  emit_phase(DocumentState::Parsed, phaseDocuments);

  std::vector<std::size_t> contentIndexes;
  contentIndexes.reserve(entries.size());
  for (std::size_t index = 0; index < entries.size(); ++index) {
    if (entries[index].document != nullptr &&
        entries[index].document->state < DocumentState::IndexedContent) {
      contentIndexes.push_back(index);
    }
  }
  std::vector<std::vector<workspace::AstNodeDescription>> exportedSymbols(
      entries.size());
  if (taskScheduler == nullptr || contentIndexes.size() <= 1) {
    for (const auto index : contentIndexes) {
      utils::throw_if_cancelled(cancelToken);
      exportedSymbols[index] =
          entries[index].services->references.scopeComputation
              ->collectExportedSymbols(*entries[index].document, cancelToken);
    }
  } else if (!contentIndexes.empty()) {
    taskScheduler->parallelFor(
        cancelToken, contentIndexes,
        [&entries, &exportedSymbols](execution::TaskScheduler::Scope &scope,
                                     std::size_t index) {
          exportedSymbols[index] =
              entries[index].services->references.scopeComputation
                  ->collectExportedSymbols(*entries[index].document,
                                           scope.cancellationToken());
        });
  }
  phaseDocuments.clear();
  for (const auto index : contentIndexes) {
    if (sharedCoreServices.workspace.indexManager != nullptr) {
      sharedCoreServices.workspace.indexManager->setExports(
          entries[index].document->id, std::move(exportedSymbols[index]));
    }
    entries[index].document->state = DocumentState::IndexedContent;
    emit_document_phase(DocumentState::IndexedContent, entries[index].document);
  }
  for (const auto &entry : entries) {
    if (entry.document != nullptr &&
        entry.document->state == DocumentState::IndexedContent) {
      phaseDocuments.push_back(entry.document);
    }
  }
  emit_phase(DocumentState::IndexedContent, phaseDocuments);

  if (!run_phase_parallel_or_inline(
          taskScheduler, entries, DocumentState::ComputedScopes, phaseDocuments,
          cancelToken,
          [](BuildEntry &entry, const utils::CancellationToken &phaseToken) {
            entry.document->localSymbols =
                entry.services->references.scopeComputation
                    ->collectLocalSymbols(*entry.document, phaseToken);
          },
          [&emit_document_phase](const std::shared_ptr<Document> &document) {
            emit_document_phase(DocumentState::ComputedScopes, document);
          })) {
    return false;
  }
  emit_phase(DocumentState::ComputedScopes, phaseDocuments);

  std::vector<BuildEntry> linkEntries;
  linkEntries.reserve(entries.size());
  {
    std::scoped_lock lock(_stateMutex);
    for (const auto &entry : entries) {
      if (!entry.document) {
        continue;
      }
      const auto effectiveOptions =
          build_options_for_document(_buildStateByDocumentId, *entry.document, options);
      if (effectiveOptions.eagerLinking) {
        linkEntries.push_back(entry);
      }
    }
  }

  if (!linkEntries.empty()) {
    if (!run_phase_parallel_or_inline(
            taskScheduler, linkEntries, DocumentState::Linked, phaseDocuments,
            cancelToken,
            [](BuildEntry &entry, const utils::CancellationToken &phaseToken) {
              entry.services->references.linker->unlink(*entry.document,
                                                        phaseToken);
              entry.services->references.linker->link(*entry.document,
                                                      phaseToken);
            },
            [&emit_document_phase](const std::shared_ptr<Document> &document) {
              emit_document_phase(DocumentState::Linked, document);
            })) {
      return false;
    }
    emit_phase(DocumentState::Linked, phaseDocuments);

    std::vector<std::size_t> referenceIndexes;
    referenceIndexes.reserve(linkEntries.size());
    for (std::size_t index = 0; index < linkEntries.size(); ++index) {
      if (linkEntries[index].document != nullptr &&
          linkEntries[index].document->state < DocumentState::IndexedReferences) {
        referenceIndexes.push_back(index);
      }
    }

    std::vector<std::vector<workspace::ReferenceDescription>> referenceDescriptions(
        linkEntries.size());
    if (taskScheduler == nullptr || referenceIndexes.size() <= 1) {
      for (const auto index : referenceIndexes) {
        utils::throw_if_cancelled(cancelToken);
        if (linkEntries[index].services->workspace.referenceDescriptionProvider !=
            nullptr) {
          referenceDescriptions[index] =
              linkEntries[index]
                  .services->workspace.referenceDescriptionProvider
                  ->createDescriptions(*linkEntries[index].document, cancelToken);
        }
      }
    } else if (!referenceIndexes.empty()) {
      taskScheduler->parallelFor(
          cancelToken, referenceIndexes,
          [&linkEntries, &referenceDescriptions](
              execution::TaskScheduler::Scope &scope, std::size_t index) {
            if (linkEntries[index].services->workspace
                    .referenceDescriptionProvider == nullptr) {
              return;
            }
            referenceDescriptions[index] =
                linkEntries[index]
                    .services->workspace.referenceDescriptionProvider
                    ->createDescriptions(*linkEntries[index].document,
                                         scope.cancellationToken());
          });
    }

    phaseDocuments.clear();
    for (const auto index : referenceIndexes) {
      linkEntries[index].document->referenceDescriptions =
          std::move(referenceDescriptions[index]);
      if (sharedCoreServices.workspace.indexManager != nullptr) {
        sharedCoreServices.workspace.indexManager->setReferences(
            linkEntries[index].document->id,
            linkEntries[index].document->referenceDescriptions);
      }
      linkEntries[index].document->state = DocumentState::IndexedReferences;
      emit_document_phase(DocumentState::IndexedReferences,
                          linkEntries[index].document);
    }
    for (const auto &entry : linkEntries) {
      if (entry.document != nullptr &&
          entry.document->state == DocumentState::IndexedReferences) {
        phaseDocuments.push_back(entry.document);
      }
    }
    emit_phase(DocumentState::IndexedReferences, phaseDocuments);
  }

  std::vector<BuildEntry> validateEntries;
  validateEntries.reserve(entries.size());
  {
    std::scoped_lock lock(_stateMutex);
    for (const auto &entry : entries) {
      if (!entry.document) {
        continue;
      }
      const auto effectiveOptions =
          build_options_for_document(_buildStateByDocumentId, *entry.document, options);
      if (effectiveOptions.validation.enabled) {
        validateEntries.push_back(entry);
      }
    }
  }

  if (!validateEntries.empty()) {
    if (!run_phase_parallel_or_inline(
            taskScheduler, validateEntries, DocumentState::Validated,
            phaseDocuments, cancelToken,
            [this, &options](BuildEntry &entry,
                             const utils::CancellationToken &phaseToken) {
                         const auto documentId = ensure_document_id(
                             sharedCoreServices, *entry.document);
                         DocumentBuildState stateCopy;
                         const DocumentBuildState *state = nullptr;
                         BuildOptions effectiveOptions = options;
                         if (documentId != InvalidDocumentId) {
                           std::scoped_lock lock(_stateMutex);
                           const auto it = _buildStateByDocumentId.find(documentId);
                           if (it != _buildStateByDocumentId.end()) {
                             stateCopy = it->second;
                             state = std::addressof(stateCopy);
                             effectiveOptions = it->second.options;
                           }
                         }

                         auto validationOptions = effectiveOptions.validation;
                         validationOptions.categories =
                             missing_validation_categories(*entry.services, state,
                                                           effectiveOptions);
                         auto validationDiagnostics =
                             entry.services->validation.documentValidator
                                 ->validateDocument(*entry.document,
                                                    validationOptions,
                                                    phaseToken);
                         if (entry.document->diagnostics.empty()) {
                           entry.document->diagnostics =
                               ParseDiagnosticsExtractor(
                                   *entry.document,
                                   entry.services != nullptr &&
                                           entry.services->parser != nullptr
                                       ? entry.services->parser.get()
                                       : nullptr)
                                   .extract(std::move(validationDiagnostics),
                                            phaseToken);
                         } else {
                           append_diagnostics(entry.document->diagnostics,
                                              std::move(validationDiagnostics));
                         }

                         if (documentId != InvalidDocumentId) {
                           std::scoped_lock lock(_stateMutex);
                           auto &buildState =
                               _buildStateByDocumentId[documentId];
                           buildState.completed = true;
                           buildState.options = effectiveOptions;
                           buildState.result.emplace();
                           auto &categories =
                               buildState.result->validationCategories;
                           if (categories.empty()) {
                             categories = validationOptions.categories;
                           } else {
                             for (const auto &category :
                                 validationOptions.categories) {
                               if (std::ranges::find(categories, category) ==
                                   categories.end()) {
                                 categories.push_back(category);
                               }
                             }
                           }
                         }
                       },
            [&emit_document_phase](const std::shared_ptr<Document> &document) {
              emit_document_phase(DocumentState::Validated, document);
            })) {
      return false;
    }
    emit_phase(DocumentState::Validated, phaseDocuments);
  }

  {
    std::unordered_set<DocumentId> validatedDocumentIds;
    validatedDocumentIds.reserve(validateEntries.size());
    for (const auto &entry : validateEntries) {
      if (entry.document && entry.document->id != InvalidDocumentId) {
        validatedDocumentIds.insert(entry.document->id);
      }
    }
    for (const auto &entry : entries) {
      if (!entry.document) {
        continue;
      }
      const auto documentId =
          ensure_document_id(sharedCoreServices, *entry.document);
      if (documentId == InvalidDocumentId ||
          validatedDocumentIds.contains(documentId)) {
        continue;
      }
      std::scoped_lock lock(_stateMutex);
      if (auto it = _buildStateByDocumentId.find(documentId);
          it != _buildStateByDocumentId.end()) {
        it->second.completed = true;
      }
    }
  }

  bool allParsed = allServicesResolved;
  for (const auto &entry : entries) {
    allParsed = entry.document && entry.document->parseSucceeded() && allParsed;
  }
  return allParsed;
}

DocumentUpdateResult
DefaultDocumentBuilder::update(std::span<const DocumentId> changedDocumentIds,
                               std::span<const DocumentId> deletedDocumentIds,
                               utils::CancellationToken cancelToken,
                               bool rebuildDocuments) const {
  utils::throw_if_cancelled(cancelToken);
  {
    std::scoped_lock lock(_stateMutex);
    _currentState = DocumentState::Changed;
  }
  _stateCv.notify_all();

  DocumentUpdateResult result;

  std::unordered_set<DocumentId> changedDocumentIdSet;
  std::unordered_set<DocumentId> seenChangedDocumentIds;
  std::unordered_set<DocumentId> seenDeletedDocumentIds;
  std::vector<DocumentId> orderedChangedDocumentIds;
  std::vector<DocumentId> orderedDeletedDocumentIds;
  std::vector<std::shared_ptr<Document>> documentsToRebuild;

  for (const auto documentId : deletedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    if (documentId != InvalidDocumentId &&
        seenDeletedDocumentIds.insert(documentId).second) {
      orderedDeletedDocumentIds.push_back(documentId);
      changedDocumentIdSet.insert(documentId);
    }
  }

  for (const auto documentId : changedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    if (documentId != InvalidDocumentId &&
        !seenDeletedDocumentIds.contains(documentId) &&
        seenChangedDocumentIds.insert(documentId).second) {
      orderedChangedDocumentIds.push_back(documentId);
      changedDocumentIdSet.insert(documentId);
    }
  }

  if (!orderedChangedDocumentIds.empty() || !orderedDeletedDocumentIds.empty()) {
    DocumentUpdateEvent updateEvent;
    updateEvent.changedDocumentIds = orderedChangedDocumentIds;
    updateEvent.deletedDocumentIds = orderedDeletedDocumentIds;
    _onUpdate.emit(updateEvent);
  }

  for (const auto documentId : orderedDeletedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    std::shared_ptr<Document> deletedDocument;
    if (sharedCoreServices.workspace.documents != nullptr) {
      deletedDocument = sharedCoreServices.workspace.documents->deleteDocument(documentId);
    }

    if (sharedCoreServices.workspace.indexManager != nullptr) {
      (void)sharedCoreServices.workspace.indexManager->remove(documentId);
    }
    {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    (void)deletedDocument;
    result.deletedDocumentIds.push_back(documentId);
  }

  for (const auto documentId : orderedChangedDocumentIds) {
    utils::throw_if_cancelled(cancelToken);
    std::shared_ptr<Document> document;
    bool materializedDocument = false;
    if (sharedCoreServices.workspace.documents != nullptr) {
      document = sharedCoreServices.workspace.documents->getDocument(documentId);
      if (document == nullptr) {
        const auto uri =
            sharedCoreServices.workspace.documents->getDocumentUri(documentId);
        if (uri.empty()) {
          continue;
        }
        document = sharedCoreServices.workspace.documents->getOrCreateDocument(
            uri, cancelToken);
        if (document == nullptr) {
          continue;
        }
        materializedDocument = true;
      }
    }
    if (!document) {
      continue;
    }
    changedDocumentIdSet.insert(document->id);

    if (!materializedDocument) {
      resetToState(*document, DocumentState::Changed);
    }
    documentsToRebuild.push_back(document);
    result.rebuiltDocuments.push_back(document);
  }

  if (rebuildDocuments) {
    std::vector<std::shared_ptr<Document>> documents;
    if (sharedCoreServices.workspace.documents != nullptr) {
      documents = utils::collect(sharedCoreServices.workspace.documents->all());
    }
    sort_documents_for_update(sharedCoreServices, documents);

    for (const auto &document : documents) {
      utils::throw_if_cancelled(cancelToken);
      if (!document || changedDocumentIdSet.contains(document->id)) {
        continue;
      }

      bool shouldRebuild = (document->state < DocumentState::Validated);
      if (!shouldRebuild) {
        DocumentBuildState buildStateCopy;
        bool hasBuildState = false;
        {
          std::scoped_lock lock(_stateMutex);
          const auto it = _buildStateByDocumentId.find(document->id);
          if (it != _buildStateByDocumentId.end()) {
            hasBuildState = true;
            buildStateCopy = it->second;
          }
        }
        if (hasBuildState) {
          shouldRebuild = !buildStateCopy.completed;
          if (!shouldRebuild) {
            shouldRebuild = validation_results_incomplete(
                std::addressof(buildStateCopy), _updateBuildOptions);
            if (shouldRebuild) {
              resetToState(*document, DocumentState::IndexedReferences);
            }
          }
        }
      }
      if (!shouldRebuild) {
        shouldRebuild = has_unresolved_references(*document);
        if (shouldRebuild) {
          resetToState(*document, DocumentState::ComputedScopes);
        }
      }
      if (!shouldRebuild) {
        shouldRebuild = sharedCoreServices.workspace.indexManager != nullptr &&
                        sharedCoreServices.workspace.indexManager->isAffected(
                            *document, changedDocumentIdSet);
        if (shouldRebuild) {
          resetToState(*document, DocumentState::ComputedScopes);
        }
      }
      if (!shouldRebuild) {
        continue;
      }
      documentsToRebuild.push_back(document);
    }

    sort_documents_for_update(sharedCoreServices, documentsToRebuild);
    result.rebuiltDocuments = documentsToRebuild;
    (void)buildInternal(documentsToRebuild, _updateBuildOptions, cancelToken,
                        false);
  }

  return result;
}

void DefaultDocumentBuilder::waitUntil(DocumentState state,
                                       utils::CancellationToken cancelToken) const {
  if (cancelToken.stop_requested()) {
    throw utils::OperationCancelled();
  }

  std::unique_lock lock(_stateMutex);
  constexpr auto kWaitStep = std::chrono::milliseconds(10);
  while (_currentState < state) {
    if (cancelToken.stop_requested()) {
      throw utils::OperationCancelled();
    }
    _stateCv.wait_for(lock, kWaitStep);
  }
}

void DefaultDocumentBuilder::waitUntil(DocumentState state,
                                       DocumentId documentId,
                                       utils::CancellationToken cancelToken) const {
  if (documentId == InvalidDocumentId || cancelToken.stop_requested()) {
    if (cancelToken.stop_requested()) {
      throw utils::OperationCancelled();
    }
    throw std::runtime_error("No document id provided.");
  }

  constexpr auto kWaitStep = std::chrono::milliseconds(10);

  while (!cancelToken.stop_requested()) {
    if (sharedCoreServices.workspace.documents == nullptr) {
      throw std::runtime_error("Document storage is not available.");
    }
    auto document = sharedCoreServices.workspace.documents->getDocument(documentId);
    if (!document) {
      throw std::runtime_error(
          std::format("No document found for id: {}", documentId));
    }
    if (document->state >= state) {
      return;
    }

    {
      std::unique_lock lock(_stateMutex);
      if (_currentState >= state) {
        throw std::runtime_error(std::format(
            "Document state of #{} is {}, requiring {}, but workspace state "
            "is already {}.",
            documentId, document_state_name(document->state),
            document_state_name(state), document_state_name(_currentState)));
      }
      _stateCv.wait_for(lock, kWaitStep);
    }
  }

  throw utils::OperationCancelled();
}

utils::ScopedDisposable DefaultDocumentBuilder::onUpdate(
    std::function<void(std::span<const DocumentId> changedDocumentIds,
                       std::span<const DocumentId> deletedDocumentIds)>
        listener) {
  return _onUpdate.on([listener = std::move(listener)](
                          const DocumentUpdateEvent &updateEvent) {
    if (listener) {
      listener(updateEvent.changedDocumentIds, updateEvent.deletedDocumentIds);
    }
  });
}

utils::ScopedDisposable DefaultDocumentBuilder::onBuildPhase(
    DocumentState targetState,
    std::function<void(std::span<const std::shared_ptr<Document>> builtDocuments)>
        listener) {
  return _onBuildPhase.on([targetState, listener = std::move(listener)](
                              const DocumentBuildPhaseEvent &phaseEvent) {
    if (listener && phaseEvent.targetState == targetState) {
      listener(phaseEvent.builtDocuments);
    }
  });
}

utils::ScopedDisposable DefaultDocumentBuilder::onDocumentPhase(
    DocumentState targetState,
    std::function<void(const std::shared_ptr<Document> &)> listener) {
  return _onDocumentPhase.on([targetState, listener = std::move(listener)](
                                 const DocumentPhaseEvent &phaseEvent) {
    if (listener && phaseEvent.targetState == targetState &&
        phaseEvent.builtDocument) {
      listener(phaseEvent.builtDocument);
    }
  });
}

void DefaultDocumentBuilder::resetToState(Document &document,
                                   DocumentState state) const {
  const auto documentId = ensure_document_id(sharedCoreServices, document);
  switch (state) {
  case DocumentState::Changed:
  case DocumentState::Parsed:
    if (sharedCoreServices.workspace.indexManager != nullptr) {
      (void)sharedCoreServices.workspace.indexManager->remove(document.id);
    }
    document.localSymbols.clear();
    document.references.clear();
    document.referenceDescriptions.clear();
    document.diagnostics.clear();
    if (documentId != InvalidDocumentId) {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    break;
  case DocumentState::IndexedContent:
    document.localSymbols.clear();
    document.references.clear();
    document.referenceDescriptions.clear();
    document.diagnostics.clear();
    if (sharedCoreServices.workspace.indexManager != nullptr) {
      sharedCoreServices.workspace.indexManager->setReferences(document.id, {});
    }
    if (documentId != InvalidDocumentId) {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    break;
  case DocumentState::ComputedScopes: {
    const auto languageServices =
        sharedCoreServices.serviceRegistry == nullptr
            ? nullptr
            : sharedCoreServices.serviceRegistry->getServicesByLanguageId(
                  document.languageId);
    if (languageServices && languageServices->references.linker) {
      languageServices->references.linker->unlink(document);
    } else {
      document.references.clear();
      document.referenceDescriptions.clear();
    }
    if (sharedCoreServices.workspace.indexManager != nullptr) {
      sharedCoreServices.workspace.indexManager->setReferences(document.id, {});
    }
    document.diagnostics.clear();
    if (documentId != InvalidDocumentId) {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    break;
  }
  case DocumentState::Linked:
    if (sharedCoreServices.workspace.indexManager != nullptr) {
      sharedCoreServices.workspace.indexManager->setReferences(document.id, {});
    }
    document.diagnostics.clear();
    if (documentId != InvalidDocumentId) {
      std::scoped_lock lock(_stateMutex);
      _buildStateByDocumentId.erase(documentId);
    }
    break;
  case DocumentState::IndexedReferences:
    document.diagnostics.clear();
    break;
  case DocumentState::Validated:
    break;
  }

  if (document.state > state) {
    document.state = state;
  }
  if (documentId != InvalidDocumentId) {
    std::scoped_lock lock(_stateMutex);
    if (const auto it = _buildStateByDocumentId.find(documentId);
        it != _buildStateByDocumentId.end()) {
      it->second.completed = (document.state >= DocumentState::Validated);
    }
  }
}

} // namespace pegium::workspace
