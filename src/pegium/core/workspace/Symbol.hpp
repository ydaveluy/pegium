#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
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

#include <pegium/core/utils/TransparentStringHash.hpp>
#include <pegium/core/syntax-tree/CstNode.hpp>
#include <pegium/core/syntax-tree/ReferenceInfo.hpp>

namespace pegium {
struct AstNode;
}

namespace pegium::workspace {

/// Stable identifier of one managed document.
using DocumentId = std::uint32_t;
inline constexpr DocumentId InvalidDocumentId =
    std::numeric_limits<DocumentId>::max();
/// Stable identifier of one AST node within a document.
using SymbolId = std::uint32_t;
inline constexpr SymbolId InvalidSymbolId =
    std::numeric_limits<SymbolId>::max();

/// Stable exported symbol description stored in workspace indexes.
struct AstNodeDescription {
  std::string name;
  std::type_index type = std::type_index(typeid(void));
  DocumentId documentId = InvalidDocumentId;
  SymbolId symbolId = InvalidSymbolId;
  /// Byte offset of the visible symbol name in the source document.
  TextOffset offset = 0;
  /// Length in bytes of the visible symbol name in the source document.
  TextOffset nameLength = 0;
};

/// Name-indexed scope entries preserving the first declaration separately.
struct NamedScopeEntries {
  const AstNodeDescription *first = nullptr;
  std::vector<const AstNodeDescription *> duplicates;

  void add(const AstNodeDescription &entry) {
    if (first == nullptr) {
      first = std::addressof(entry);
      return;
    }
    duplicates.push_back(std::addressof(entry));
  }

  [[nodiscard]] bool empty() const noexcept { return first == nullptr; }
};

/// Scope entries grouped by assignable target type.
///
/// `ownedEntries` provides stable storage for the descriptions; `entriesByName`
/// indexes them by name. `std::deque` is used so that adding a new description
/// never invalidates the pointers held by the name index.
struct ScopeEntryBucket {
  using NameIndex =
      std::unordered_map<std::string_view,
                         NamedScopeEntries,
                         utils::TransparentStringHash, std::equal_to<>>;

  std::type_index type = std::type_index(typeid(void));
  std::deque<AstNodeDescription> ownedEntries;
  NameIndex entriesByName;
};

/// Collection of typed scope entry buckets.
///
/// `std::deque` keeps existing buckets at stable addresses when a new one is
/// appended, so the description pointers held by `ScopeEntryBucket::entriesByName`
/// remain valid. In practice each container has only a handful of distinct
/// symbol types, so a linear scan over the buckets is faster than maintaining a
/// side index.
using BucketedScopeEntries = std::deque<ScopeEntryBucket>;

/// Stable key identifying one AST node across workspace indexes.
struct NodeKey {
  DocumentId documentId = InvalidDocumentId;
  SymbolId symbolId = InvalidSymbolId;

  [[nodiscard]] bool empty() const noexcept {
    return documentId == InvalidDocumentId || symbolId == InvalidSymbolId;
  }

  [[nodiscard]] bool operator==(const NodeKey &) const noexcept = default;
};

/// Hash functor for `NodeKey`.
struct NodeKeyHash {
  [[nodiscard]] std::size_t operator()(const NodeKey &key) const noexcept {
    const auto documentHash = std::hash<DocumentId>{}(key.documentId);
    const auto symbolHash = std::hash<SymbolId>{}(key.symbolId);
    return documentHash ^ (symbolHash + 0x9e3779b9u + (documentHash << 6u) +
                           (documentHash >> 2u));
  }
};

/// Indexed reference occurrence stored by the workspace.
struct ReferenceDescription {
  DocumentId sourceDocumentId = InvalidDocumentId;
  TextOffset sourceOffset = 0;
  TextOffset sourceLength = 0;

  std::type_index referenceType = std::type_index(typeid(void));
  bool local = false;
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

/// Categorization of why a reference failed to resolve. The diagnostic message
/// is built on demand from the reference's metadata via
/// `AbstractReference::getErrorMessage`.
enum class LinkingErrorKind : std::uint8_t {
  /// Document hasn't reached `ComputedScopes` yet — the reference stays
  /// `Unresolved` and the linker retries later.
  Retryable,
  /// Scope provider returned no candidate matching the reference text.
  NotFound,
  /// `CyclicReferenceResolution` thrown during resolution.
  Cycle,
  /// Resolver caught a `std::exception` (full text logged via observability).
  Exception,
};

/// Linking failure payload returned by reference resolution helpers.
struct LinkingError {
  ReferenceInfo info;
  LinkingErrorKind kind = LinkingErrorKind::NotFound;
};

/// Fully resolved AST node together with its stable description.
///
/// `description` is a non-owning pointer into stable scope storage (the
/// `std::deque<AstNodeDescription>` inside `ScopeEntryBucket::ownedEntries`).
/// Pointer stability is guaranteed for the lifetime of the source document's
/// `LocalSymbols` / global index entries — which is the same lifetime callers
/// already assume for `Reference` resolutions.
struct ResolvedAstNodeDescription {
  const AstNode *node = nullptr;
  const AstNodeDescription *description = nullptr;
};

/// Result of resolving one symbol description.
using AstNodeDescriptionOrError = std::variant<AstNodeDescription, LinkingError>;
/// Result of resolving multiple symbol descriptions.
using AstNodeDescriptionsOrError =
    std::variant<std::vector<AstNodeDescription>, LinkingError>;
/// Result of resolving one symbol description to a live AST node.
using ResolvedAstNodeDescriptionOrError =
    std::variant<ResolvedAstNodeDescription, LinkingError>;
/// Result of resolving multiple descriptions to live AST nodes.
using ResolvedAstNodeDescriptionsOrError =
    std::variant<std::vector<ResolvedAstNodeDescription>, LinkingError>;

} // namespace pegium::workspace
