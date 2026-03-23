#pragma once

#include <optional>
#include <vector>

#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <pegium/core/workspace/Symbol.hpp>

namespace pegium::references {

/// Additional filters used by `References::findReferences`.
struct FindReferencesOptions {
  /// Restricts results to one source document when set.
  std::optional<workspace::DocumentId> documentId;
  /// Includes the declaration span of `targetNode` alongside incoming references.
  bool includeDeclaration = false;
};

/// High-level cross-reference queries used by navigation features.
class References {
public:
  virtual ~References() noexcept = default;

  /// Returns the declaration AST nodes matching `sourceCstNode`.
  ///
  /// `sourceCstNode` must be valid and attached to a managed workspace
  /// document.
  ///
  /// Returned pointers are never null and always belong to a managed
  /// workspace document. When no declaration matches, the result is empty.
  [[nodiscard]] virtual std::vector<const AstNode *>
  findDeclarations(const CstNodeView &sourceCstNode) const = 0;

  /// Returns declaration CST nodes for `sourceCstNode`.
  ///
  /// `sourceCstNode` must be valid and attached to a managed workspace
  /// document.
  ///
  /// Returned views are always valid, attached to a managed workspace
  /// document, and never fall back to an empty placeholder. When no
  /// declaration matches, the result is empty.
  [[nodiscard]] virtual std::vector<CstNodeView>
  findDeclarationNodes(const CstNodeView &sourceCstNode) const = 0;

  /// Returns all references targeting `targetNode`.
  ///
  /// `targetNode` must belong to a managed workspace document.
  [[nodiscard]] virtual std::vector<workspace::ReferenceDescription>
  findReferences(const AstNode &targetNode,
                 const FindReferencesOptions &options) const = 0;
};

} // namespace pegium::references
