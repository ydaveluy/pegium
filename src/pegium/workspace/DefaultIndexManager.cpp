#include <pegium/workspace/DefaultIndexManager.hpp>

#include <ranges>
#include <typeindex>
#include <unordered_map>

#include <pegium/workspace/Document.hpp>

namespace pegium::workspace {

void DefaultIndexManager::setExports(DocumentId documentId,
                                     std::vector<AstNodeDescription> exports) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    for (const auto &entry : exports) {
      if (entry.documentId != InvalidDocumentId) {
        documentId = entry.documentId;
        break;
      }
    }
  }
  if (documentId == InvalidDocumentId) {
    return;
  }

  auto scopeEntries = std::make_shared<std::vector<AstNodeDescription>>();
  scopeEntries->reserve(exports.size());
  for (auto &entry : exports) {
    entry.documentId = documentId;
    scopeEntries->emplace_back(std::move(entry));
  }
  _exportsByDocument.insert_or_assign(documentId, std::move(scopeEntries));
  _exportsByTypeCache.clear(documentId);
  _allScopeEntriesCache.reset();
  _allBucketedScopeEntriesCache.reset();
  _exportCachesDirty = true;
  ++_generation;
}

void DefaultIndexManager::setReferences(
    DocumentId documentId, std::vector<ReferenceDescription> references) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    for (const auto &reference : references) {
      if (reference.sourceDocumentId != InvalidDocumentId) {
        documentId = reference.sourceDocumentId;
        break;
      }
    }
  }
  if (documentId == InvalidDocumentId) {
    return;
  }

  for (auto &reference : references) {
    reference.sourceDocumentId = documentId;
  }

  _referencesByDocument.insert_or_assign(documentId, std::move(references));
  _referenceTargetCacheDirty = true;
  ++_generation;
}

bool DefaultIndexManager::remove(DocumentId documentId) {
  std::scoped_lock lock(_mutex);
  if (documentId == InvalidDocumentId) {
    return false;
  }

  const bool removedExports = _exportsByDocument.erase(documentId) > 0;
  const bool removedReferences = _referencesByDocument.erase(documentId) > 0;
  if (removedExports) {
    _exportsByTypeCache.clear(documentId);
    _allScopeEntriesCache.reset();
    _allBucketedScopeEntriesCache.reset();
    _exportCachesDirty = true;
  }
  if (removedReferences) {
    _referenceTargetCacheDirty = true;
  }
  if (removedExports || removedReferences) {
    ++_generation;
  }
  return removedExports || removedReferences;
}

std::shared_ptr<const BucketedScopeEntries>
DefaultIndexManager::allBucketedScopeEntries() const {
  std::scoped_lock lock(_mutex);
  rebuildExportCachesLocked();
  return _allBucketedScopeEntriesCache;
}

std::uint64_t DefaultIndexManager::generation() const noexcept {
  std::scoped_lock lock(_mutex);
  return _generation;
}

utils::stream<AstNodeDescription>
DefaultIndexManager::allElements() const {
  std::vector<AstNodeDescription> result;
  if (const auto entries = allScopeEntries(); entries != nullptr) {
    result.reserve(entries->entries.size());
    for (const auto *entry : entries->entries) {
      if (entry != nullptr) {
        result.push_back(*entry);
      }
    }
  }
  return utils::make_stream<AstNodeDescription>(std::move(result));
}

utils::stream<AstNodeDescription>
DefaultIndexManager::allElements(std::type_index type) const {
  std::vector<AstNodeDescription> result;
  if (const auto entries = allScopeEntries(type); entries != nullptr) {
    result.reserve(entries->entries.size());
    for (const auto *entry : entries->entries) {
      if (entry != nullptr) {
        result.push_back(*entry);
      }
    }
  }
  return utils::make_stream<AstNodeDescription>(std::move(result));
}

std::shared_ptr<const IndexedScopeEntries>
DefaultIndexManager::allScopeEntries() const {
  std::scoped_lock lock(_mutex);
  rebuildExportCachesLocked();
  return _allScopeEntriesCache;
}

std::shared_ptr<const IndexedScopeEntries>
DefaultIndexManager::allScopeEntries(std::type_index type) const {
  std::scoped_lock lock(_mutex);
  auto result = std::make_shared<IndexedScopeEntries>();
  result->owners.reserve(_exportsByDocument.size());
  for (const auto &[documentId, exports] : _exportsByDocument) {
    if (exports == nullptr || exports->empty()) {
      continue;
    }
    const auto filteredByDocument =
        scopeEntriesForDocumentLocked(documentId, type);
    if (filteredByDocument == nullptr || filteredByDocument->empty()) {
      continue;
    }
    result->owners.push_back(exports);
    result->entries.insert(result->entries.end(), filteredByDocument->begin(),
                           filteredByDocument->end());
  }
  return result;
}

std::shared_ptr<const std::vector<const AstNodeDescription *>>
DefaultIndexManager::scopeEntriesForDocumentLocked(DocumentId documentId,
                                                   std::type_index type) const {
  return _exportsByTypeCache.get(
      documentId, type,
      [this, documentId, type] {
        auto filtered = std::make_shared<std::vector<const AstNodeDescription *>>();
        const auto exportsIt = _exportsByDocument.find(documentId);
        if (exportsIt == _exportsByDocument.end() ||
            exportsIt->second == nullptr) {
          return filtered;
        }
        const auto &exports = *exportsIt->second;
        filtered->reserve(exports.size());
        for (const auto &description : exports) {
          if (description.type == type) {
            filtered->push_back(std::addressof(description));
          }
        }
        return filtered;
      });
}

utils::stream<AstNodeDescription>
DefaultIndexManager::findElementsByName(std::string_view name) const {
  std::scoped_lock lock(_mutex);
  rebuildExportCachesLocked();

  const auto it = _exportsByNameCache.find(std::string(name));
  if (it == _exportsByNameCache.end()) {
    return utils::make_stream<AstNodeDescription>(
        std::views::empty<AstNodeDescription>);
  }
  return utils::make_stream<AstNodeDescription>(it->second);
}

utils::stream<AstNodeDescription>
DefaultIndexManager::elementsForDocument(DocumentId documentId) const {
  std::scoped_lock lock(_mutex);

  const auto it = _exportsByDocument.find(documentId);
  if (it == _exportsByDocument.end() || it->second == nullptr) {
    return utils::make_stream<AstNodeDescription>(
        std::views::empty<AstNodeDescription>);
  }
  std::vector<AstNodeDescription> result;
  result.reserve(it->second->size());
  for (const auto &entry : *it->second) {
    result.push_back(entry);
  }
  return utils::make_stream<AstNodeDescription>(std::move(result));
}

utils::stream<ReferenceDescription>
DefaultIndexManager::referenceDescriptionsForDocument(
    DocumentId documentId) const {
  std::scoped_lock lock(_mutex);

  const auto it = _referencesByDocument.find(documentId);
  if (it == _referencesByDocument.end()) {
    return utils::make_stream<ReferenceDescription>(
        std::views::empty<ReferenceDescription>);
  }
  return utils::make_stream<ReferenceDescription>(it->second);
}

utils::stream<ReferenceDescriptionOrDeclaration>
DefaultIndexManager::findAllReferences(const NodeKey &targetKey,
                                       bool includeDeclaration) const {
  if (targetKey.empty()) {
    return utils::make_stream<ReferenceDescriptionOrDeclaration>(
        std::views::empty<ReferenceDescriptionOrDeclaration>);
  }

  std::scoped_lock lock(_mutex);
  rebuildReferenceTargetCacheLocked();

  std::vector<ReferenceDescriptionOrDeclaration> results;
  if (const auto refsIt = _referencesByTargetKeyCache.find(targetKey);
      refsIt != _referencesByTargetKeyCache.end()) {
    results.reserve(refsIt->second.size() + 1);
    for (const auto &reference : refsIt->second) {
      results.emplace_back(reference);
    }
  }

  if (!includeDeclaration) {
    return utils::make_stream<ReferenceDescriptionOrDeclaration>(
        std::move(results));
  }

  const auto exportsIt = _exportsByDocument.find(targetKey.documentId);
  if (exportsIt == _exportsByDocument.end() || exportsIt->second == nullptr) {
    return utils::make_stream<ReferenceDescriptionOrDeclaration>(
        std::move(results));
  }
  for (const auto &description : *exportsIt->second) {
    if (description.symbolId == targetKey.symbolId) {
      results.emplace_back(description);
      break;
    }
  }

  return utils::make_stream<ReferenceDescriptionOrDeclaration>(
      std::move(results));
}

bool DefaultIndexManager::isAffected(
    const Document &document,
    const std::unordered_set<DocumentId> &changedDocumentIds) const {
  if (changedDocumentIds.empty()) {
    return false;
  }

  std::scoped_lock lock(_mutex);
  const auto documentId = document.id;
  const auto referencesIt = _referencesByDocument.find(documentId);
  if (referencesIt == _referencesByDocument.end()) {
    return false;
  }

  for (const auto &reference : referencesIt->second) {
    if (reference.targetDocumentId.has_value() &&
        changedDocumentIds.contains(*reference.targetDocumentId)) {
      return true;
    }
  }

  return false;
}

void DefaultIndexManager::rebuildExportCachesLocked() const {
  if (!_exportCachesDirty) {
    return;
  }

  auto allScopeEntries = std::make_shared<IndexedScopeEntries>();
  auto allBucketedScopeEntries = std::make_shared<BucketedScopeEntries>();
  _exportsByNameCache.clear();

  std::size_t exportCount = 0;
  for (const auto &[documentId, exports] : _exportsByDocument) {
    (void)documentId;
    if (exports != nullptr) {
      exportCount += exports->size();
    }
  }
  allScopeEntries->owners.reserve(_exportsByDocument.size());
  allScopeEntries->entries.reserve(exportCount);
  allBucketedScopeEntries->owners.reserve(_exportsByDocument.size());
  _exportsByNameCache.reserve(exportCount);

  std::unordered_map<std::type_index, std::size_t> bucketIndexByType;
  bucketIndexByType.reserve(exportCount);

  for (const auto &[documentId, exports] : _exportsByDocument) {
    (void)documentId;
    if (exports == nullptr || exports->empty()) {
      continue;
    }

    allScopeEntries->owners.push_back(exports);
    allBucketedScopeEntries->owners.push_back(exports);
    for (const auto &description : *exports) {
      const auto *entry = std::addressof(description);
      allScopeEntries->entries.push_back(entry);
      _exportsByNameCache[description.name].push_back(description);

      const auto semanticType = description.type;
      const auto [bucketIt, inserted] =
          bucketIndexByType.try_emplace(semanticType,
                                        allBucketedScopeEntries->buckets.size());
      if (inserted) {
        auto &bucket = allBucketedScopeEntries->buckets.emplace_back();
        bucket.type = semanticType;
        bucket.representative = entry;
      }

      auto &bucket = allBucketedScopeEntries->buckets[bucketIt->second];
      bucket.entries.push_back(entry);
      bucket.entriesByName.emplace(entry->name, entry);
      if (bucket.representative == nullptr) {
        bucket.representative = entry;
      }
    }
  }

  _allScopeEntriesCache = std::move(allScopeEntries);
  _allBucketedScopeEntriesCache = std::move(allBucketedScopeEntries);
  _exportCachesDirty = false;
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
