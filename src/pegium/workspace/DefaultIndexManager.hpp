#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/utils/Caching.hpp>
#include <pegium/workspace/IndexManager.hpp>

namespace pegium::workspace {

class DefaultIndexManager : public IndexManager {
public:
  DefaultIndexManager() = default;

  void setExports(DocumentId documentId,
                  std::vector<AstNodeDescription> exports) override;

  void
  setReferences(DocumentId documentId,
                std::vector<ReferenceDescription> references) override;

  bool remove(DocumentId documentId) override;

  [[nodiscard]] std::shared_ptr<const BucketedScopeEntries>
  allBucketedScopeEntries() const override;
  [[nodiscard]] std::shared_ptr<const IndexedScopeEntries>
  allScopeEntries() const override;
  [[nodiscard]] std::shared_ptr<const IndexedScopeEntries>
  allScopeEntries(std::type_index type) const override;
  [[nodiscard]] utils::stream<AstNodeDescription>
  allElements() const override;
  [[nodiscard]] utils::stream<AstNodeDescription>
  allElements(std::type_index type) const override;

  [[nodiscard]] utils::stream<AstNodeDescription>
  findElementsByName(std::string_view name) const override;

  [[nodiscard]] utils::stream<AstNodeDescription>
  elementsForDocument(DocumentId documentId) const override;

  [[nodiscard]] utils::stream<ReferenceDescription>
  referenceDescriptionsForDocument(DocumentId documentId) const override;
  [[nodiscard]] utils::stream<ReferenceDescriptionOrDeclaration>
  findAllReferences(const NodeKey &targetKey, bool includeDeclaration) const override;
  [[nodiscard]] std::uint64_t generation() const noexcept override;

  [[nodiscard]] bool
  isAffected(const Document &document,
             const std::unordered_set<DocumentId> &changedDocumentIds) const override;

private:
  void rebuildExportCachesLocked() const;
  void rebuildReferenceTargetCacheLocked() const;
  [[nodiscard]] std::shared_ptr<const std::vector<const AstNodeDescription *>>
  scopeEntriesForDocumentLocked(DocumentId documentId,
                                std::type_index type) const;

  mutable std::mutex _mutex;
  std::unordered_map<DocumentId, std::shared_ptr<const std::vector<AstNodeDescription>>>
      _exportsByDocument;
  std::unordered_map<DocumentId, std::vector<ReferenceDescription>>
      _referencesByDocument;

  mutable bool _exportCachesDirty = true;
  mutable bool _referenceTargetCacheDirty = true;
  mutable std::shared_ptr<const IndexedScopeEntries> _allScopeEntriesCache;
  mutable std::shared_ptr<const BucketedScopeEntries> _allBucketedScopeEntriesCache;
  mutable utils::ContextCache<DocumentId, std::type_index,
                              std::shared_ptr<const std::vector<const AstNodeDescription *>>>
      _exportsByTypeCache;
  mutable std::unordered_map<std::string, std::vector<AstNodeDescription>>
      _exportsByNameCache;
  mutable std::unordered_map<NodeKey, std::vector<ReferenceDescription>,
                             NodeKeyHash>
      _referencesByTargetKeyCache;
  std::uint64_t _generation = 0;
};

} // namespace pegium::workspace
