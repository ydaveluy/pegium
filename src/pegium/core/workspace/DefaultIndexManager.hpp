#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <span>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/core/services/DefaultSharedCoreService.hpp>
#include <pegium/core/utils/Caching.hpp>
#include <pegium/core/workspace/IndexManager.hpp>

namespace pegium::workspace {

/// Default workspace index storing exported symbols and reference descriptions.
class DefaultIndexManager : public IndexManager,
                            protected pegium::DefaultSharedCoreService {
public:
  explicit DefaultIndexManager(pegium::SharedCoreServices &sharedServices);

  void updateContent(Document &document,
                     utils::CancellationToken cancelToken) override;

  void updateReferences(Document &document,
                        utils::CancellationToken cancelToken) override;

  bool removeContent(DocumentId documentId) override;

  bool removeReferences(DocumentId documentId) override;

  bool remove(DocumentId documentId) override;

  [[nodiscard]] std::vector<AstNodeDescription>
  allElements(std::optional<std::type_index> type = std::nullopt,
              std::span<const DocumentId> documentIds = {}) const override;
  [[nodiscard]] std::optional<AstNodeDescription>
  findByName(std::string_view name,
             std::optional<std::type_index> type = std::nullopt) const override;
  [[nodiscard]] std::vector<ReferenceDescription>
  findAllReferences(const NodeKey &targetKey) const override;

  [[nodiscard]] bool isAffected(
      const Document &document,
      const std::unordered_set<DocumentId> &changedDocumentIds) const override;

private:
  void rebuildReferenceTargetCacheLocked() const;
  [[nodiscard]] std::vector<AstNodeDescription>
  getFileDescriptionsLocked(DocumentId documentId,
                            std::optional<std::type_index> type) const;
  [[nodiscard]] std::vector<AstNodeDescription>
  filterDescriptionsByTypeLocked(const std::vector<AstNodeDescription> &exports,
                                 std::type_index type) const;

  mutable std::mutex _mutex;
  // Sorted by DocumentId so iteration is inherently in a stable, deterministic
  // order — the order shared by allElements() and findByName(). This is why a
  // std::map is used rather than an unordered_map (whose iteration order is
  // unspecified): the determinism is structural, not re-imposed per call.
  std::map<DocumentId, std::vector<AstNodeDescription>> _exportsByDocument;
  std::unordered_map<DocumentId, std::vector<ReferenceDescription>>
      _referencesByDocument;

  mutable bool _referenceTargetCacheDirty = true;
  mutable utils::ContextCache<DocumentId, std::type_index,
                              std::vector<AstNodeDescription>>
      _exportsByTypeCache;
  mutable std::unordered_map<NodeKey, std::vector<ReferenceDescription>,
                             NodeKeyHash>
      _referencesByTargetKeyCache;
};

} // namespace pegium::workspace
