#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <variant>
#include <vector>

#include <pegium/syntax-tree/CstNode.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

using DocumentId = std::uint32_t;
inline constexpr DocumentId InvalidDocumentId =
    std::numeric_limits<DocumentId>::max();
using SymbolId = std::uint32_t;
inline constexpr SymbolId InvalidSymbolId =
    std::numeric_limits<SymbolId>::max();

struct AstNodeDescription {
  std::string name;
  const AstNode *node = nullptr;
  std::type_index type = std::type_index(typeid(void));
  DocumentId documentId = InvalidDocumentId;
  SymbolId symbolId = InvalidSymbolId;
  TextOffset offset = 0;
  TextOffset nameLength = 0;
};

struct IndexedScopeEntries {
  std::vector<std::shared_ptr<const std::vector<AstNodeDescription>>> owners;
  std::vector<const AstNodeDescription *> entries;
};

struct ScopeEntryBucket {
  using NameIndex =
      std::unordered_multimap<std::string_view, const AstNodeDescription *>;

  std::type_index type = std::type_index(typeid(void));
  const AstNodeDescription *representative = nullptr;
  std::vector<const AstNodeDescription *> entries;
  NameIndex entriesByName;
};

struct BucketedScopeEntries {
  std::vector<std::shared_ptr<const std::vector<AstNodeDescription>>> owners;
  std::vector<ScopeEntryBucket> buckets;
};

struct NodeKey {
  DocumentId documentId = InvalidDocumentId;
  SymbolId symbolId = InvalidSymbolId;

  [[nodiscard]] bool empty() const noexcept {
    return documentId == InvalidDocumentId || symbolId == InvalidSymbolId;
  }

  [[nodiscard]] bool operator==(const NodeKey &) const noexcept = default;
};

struct NodeKeyHash {
  [[nodiscard]] std::size_t operator()(const NodeKey &key) const noexcept {
    const auto documentHash = std::hash<DocumentId>{}(key.documentId);
    const auto symbolHash = std::hash<SymbolId>{}(key.symbolId);
    return documentHash ^ (symbolHash + 0x9e3779b9u + (documentHash << 6u) +
                           (documentHash >> 2u));
  }
};

struct ReferenceDescription {
  DocumentId sourceDocumentId = InvalidDocumentId;
  TextOffset sourceOffset = 0;
  TextOffset sourceLength = 0;

  std::type_index referenceType = std::type_index(typeid(void));
  std::optional<DocumentId> targetDocumentId;
  std::optional<SymbolId> targetSymbolId;

  [[nodiscard]] bool isResolved() const noexcept {
    return targetDocumentId.has_value() && *targetDocumentId != InvalidDocumentId &&
           targetSymbolId.has_value() &&
           *targetSymbolId != InvalidSymbolId;
  }

  [[nodiscard]] std::string_view
  sourceText(std::string_view documentText) const noexcept {
    const auto begin = static_cast<std::size_t>(sourceOffset);
    if (begin >= documentText.size() || sourceLength == 0) {
      return {};
    }
    const auto available = documentText.size() - begin;
    const auto length =
        std::min<std::size_t>(available, static_cast<std::size_t>(sourceLength));
    return documentText.substr(begin, length);
  }

  [[nodiscard]] std::optional<NodeKey> targetKey() const {
    if (!isResolved() || !targetDocumentId.has_value() ||
        *targetDocumentId == InvalidDocumentId) {
      return std::nullopt;
    }
    return NodeKey{.documentId = *targetDocumentId,
                   .symbolId = *targetSymbolId};
  }
};

using ReferenceDescriptionOrDeclaration =
    std::variant<ReferenceDescription, AstNodeDescription>;

} // namespace pegium::workspace
