#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <pegium/utils/Stream.hpp>
#include <pegium/workspace/Symbol.hpp>

namespace pegium::workspace {

struct Document;

class IndexManager {
public:
  virtual ~IndexManager() noexcept = default;
  virtual void setExports(DocumentId documentId,
                          std::vector<AstNodeDescription> exports) = 0;
  virtual void
  setReferences(DocumentId documentId,
                std::vector<ReferenceDescription> references) = 0;
  virtual bool remove(DocumentId documentId) = 0;

  [[nodiscard]] virtual std::shared_ptr<const BucketedScopeEntries>
  allBucketedScopeEntries() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const IndexedScopeEntries>
  allScopeEntries() const = 0;
  [[nodiscard]] virtual std::shared_ptr<const IndexedScopeEntries>
  allScopeEntries(std::type_index type) const = 0;
  [[nodiscard]] virtual utils::stream<AstNodeDescription>
  allElements() const = 0;
  [[nodiscard]] virtual utils::stream<AstNodeDescription>
  allElements(std::type_index type) const = 0;
  [[nodiscard]] virtual utils::stream<AstNodeDescription>
  findElementsByName(std::string_view name) const = 0;
  [[nodiscard]] virtual utils::stream<AstNodeDescription>
  elementsForDocument(DocumentId documentId) const = 0;
  [[nodiscard]] virtual utils::stream<ReferenceDescription>
  referenceDescriptionsForDocument(DocumentId documentId) const = 0;
  [[nodiscard]] virtual utils::stream<ReferenceDescriptionOrDeclaration>
  findAllReferences(const NodeKey &targetKey, bool includeDeclaration) const = 0;
  [[nodiscard]] virtual std::uint64_t generation() const noexcept = 0;

  [[nodiscard]] virtual bool
  isAffected(const Document &document,
             const std::unordered_set<DocumentId> &changedDocumentIds) const = 0;
};

} // namespace pegium::workspace
