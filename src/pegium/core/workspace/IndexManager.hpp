#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <typeindex>
#include <unordered_set>

#include <pegium/core/utils/Cancellation.hpp>
#include <pegium/core/workspace/Symbol.hpp>

namespace pegium::workspace {

struct Document;

/// Indexes exported symbols and resolved references across workspace documents.
class IndexManager {
public:
  virtual ~IndexManager() noexcept = default;
  /// Recomputes exported content symbols for `document`.
  virtual void updateContent(Document &document,
                             utils::CancellationToken cancelToken) = 0;
  /// Recomputes reference descriptions for `document`.
  virtual void updateReferences(Document &document,
                                utils::CancellationToken cancelToken) = 0;
  /// Removes exported content previously stored for `documentId`.
  virtual bool removeContent(DocumentId documentId) = 0;
  /// Removes references previously stored for `documentId`.
  virtual bool removeReferences(DocumentId documentId) = 0;
  /// Removes every indexed contribution of `documentId`.
  virtual bool remove(DocumentId documentId) = 0;

  /// Returns all indexed exported symbols, optionally filtered by type or document ids.
  [[nodiscard]] virtual std::vector<AstNodeDescription>
  allElements(std::optional<std::type_index> type = std::nullopt,
              std::span<const DocumentId> documentIds = {}) const = 0;
  /// Returns every indexed reference targeting `targetKey`.
  [[nodiscard]] virtual std::vector<ReferenceDescription>
  findAllReferences(const NodeKey &targetKey) const = 0;

  /// Returns whether `document` may need relinking after the given changes.
  [[nodiscard]] virtual bool isAffected(
      const Document &document,
      const std::unordered_set<DocumentId> &changedDocumentIds) const = 0;
};

} // namespace pegium::workspace
