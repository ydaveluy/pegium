#include <pegium/core/workspace/DefaultIndexManager.hpp>

#include <algorithm>
#include <ranges>
#include <typeinfo>

#include <pegium/core/services/ServiceRegistry.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {

namespace {

bool matches_type(const AstNodeDescription &description,
                  std::type_index expectedType,
                  const AstReflection &reflection) {
  if (expectedType == std::type_index(typeid(void)) ||
      description.type == std::type_index(typeid(void))) {
    return false;
  }
  if (description.type == expectedType) {
    return true;
  }
  return reflection.isSubtype(description.type, expectedType);
}

} // namespace

DefaultIndexManager::DefaultIndexManager(
    services::SharedCoreServices &sharedServices)
    : services::DefaultSharedCoreService(sharedServices) {}

void DefaultIndexManager::updateContent(Document &document,
                                        utils::CancellationToken cancelToken) {
  const auto &services =
      shared.serviceRegistry->getServices(document.uri);
  auto exports = services.references.scopeComputation->collectExportedSymbols(
      document, cancelToken);

  std::scoped_lock lock(_mutex);
  _exportsByDocument.insert_or_assign(document.id, std::move(exports));
  _exportsByTypeCache.clear(document.id);
}

void DefaultIndexManager::updateReferences(
    Document &document, utils::CancellationToken cancelToken) {
  const auto &services =
      shared.serviceRegistry->getServices(document.uri);
  auto descriptions =
      services.workspace.referenceDescriptionProvider->createDescriptions(
          document, cancelToken);

  for (auto &reference : descriptions) {
    reference.sourceDocumentId = document.id;
  }

  std::scoped_lock lock(_mutex);
  _referencesByDocument.insert_or_assign(document.id, std::move(descriptions));
  _referenceTargetCacheDirty = true;
}

bool DefaultIndexManager::removeContent(DocumentId documentId) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    return false;
  }

  const bool removed = _exportsByDocument.erase(documentId) > 0;
  if (removed) {
    _exportsByTypeCache.clear(documentId);
  }
  return removed;
}

bool DefaultIndexManager::removeReferences(DocumentId documentId) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    return false;
  }

  const bool removed = _referencesByDocument.erase(documentId) > 0;
  if (removed) {
    _referenceTargetCacheDirty = true;
  }
  return removed;
}

bool DefaultIndexManager::remove(DocumentId documentId) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    return false;
  }

  const bool removedContent = _exportsByDocument.erase(documentId) > 0;
  const bool removedReferences = _referencesByDocument.erase(documentId) > 0;
  if (removedContent) {
    _exportsByTypeCache.clear(documentId);
  }
  if (removedReferences) {
    _referenceTargetCacheDirty = true;
  }
  return removedContent || removedReferences;
}

std::vector<AstNodeDescription> DefaultIndexManager::allElements(
    std::optional<std::type_index> type,
    std::span<const DocumentId> documentIds) const {
  std::vector<AstNodeDescription> result;

  std::scoped_lock lock(_mutex);
  if (documentIds.empty()) {
    std::vector<DocumentId> indexedDocumentIds;
    indexedDocumentIds.reserve(_exportsByDocument.size());
    for (const auto &[documentId, exports] : _exportsByDocument) {
      (void)exports;
      indexedDocumentIds.push_back(documentId);
    }
    std::ranges::sort(indexedDocumentIds);
    for (const auto documentId : indexedDocumentIds) {
      auto descriptions = getFileDescriptionsLocked(documentId, type);
      result.insert(result.end(), descriptions.begin(), descriptions.end());
    }
    return result;
  }

  for (const auto documentId : documentIds) {
    auto descriptions = getFileDescriptionsLocked(documentId, type);
    result.insert(result.end(), descriptions.begin(), descriptions.end());
  }
  return result;
}

std::vector<ReferenceDescription>
DefaultIndexManager::findAllReferences(const NodeKey &targetKey) const {
  if (targetKey.empty()) {
    return {};
  }

  std::scoped_lock lock(_mutex);
  rebuildReferenceTargetCacheLocked();

  const auto refsIt = _referencesByTargetKeyCache.find(targetKey);
  if (refsIt == _referencesByTargetKeyCache.end()) {
    return {};
  }
  return std::vector<ReferenceDescription>(refsIt->second.begin(),
                                           refsIt->second.end());
}

bool DefaultIndexManager::isAffected(
    const Document &document,
    const std::unordered_set<DocumentId> &changedDocumentIds) const {
  if (changedDocumentIds.empty()) {
    return false;
  }

  std::scoped_lock lock(_mutex);
  const auto referencesIt = _referencesByDocument.find(document.id);
  if (referencesIt == _referencesByDocument.end()) {
    return false;
  }

  return std::ranges::any_of(referencesIt->second, [&changedDocumentIds](
                                                      const auto &reference) {
    return !reference.local && reference.targetDocumentId.has_value() &&
           changedDocumentIds.contains(*reference.targetDocumentId);
  });
}

std::vector<AstNodeDescription> DefaultIndexManager::getFileDescriptionsLocked(
    DocumentId documentId, std::optional<std::type_index> type) const {
  const auto exportsIt = _exportsByDocument.find(documentId);
  if (exportsIt == _exportsByDocument.end()) {
    return {};
  }
  if (!type.has_value()) {
    return exportsIt->second;
  }
  return _exportsByTypeCache.get(documentId, *type, [this, documentId, type] {
    return filterDescriptionsByTypeLocked(documentId, *type);
  });
}

std::vector<AstNodeDescription>
DefaultIndexManager::filterDescriptionsByTypeLocked(DocumentId documentId,
                                                    std::type_index type) const {
  std::vector<AstNodeDescription> filtered;
  const auto exportsIt = _exportsByDocument.find(documentId);
  if (exportsIt == _exportsByDocument.end()) {
    return filtered;
  }

  const auto &reflection = *shared.astReflection;
  for (const auto &description : exportsIt->second) {
    if (matches_type(description, type, reflection)) {
      filtered.push_back(description);
    }
  }
  return filtered;
}

void DefaultIndexManager::rebuildReferenceTargetCacheLocked() const {
  if (!_referenceTargetCacheDirty) {
    return;
  }

  _referencesByTargetKeyCache.clear();
  for (const auto &[sourceDocumentId, references] : _referencesByDocument) {
    (void)sourceDocumentId;
    for (const auto &reference : references) {
      const auto targetKey = reference.targetKey();
      if (!targetKey.has_value()) {
        continue;
      }
      _referencesByTargetKeyCache[*targetKey].push_back(reference);
    }
  }

  _referenceTargetCacheDirty = false;
}

} // namespace pegium::workspace
