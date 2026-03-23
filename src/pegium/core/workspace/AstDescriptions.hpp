#pragma once

#include <pegium/core/workspace/Symbol.hpp>

namespace pegium {
struct AstNode;
namespace workspace {
class Documents;
struct Document;

/// Resolves a symbol description to the current in-memory AST node.
///
/// `documents` is required by design. `description` must be complete, with
/// valid `documentId` and `symbolId`, and it must still resolve to a live AST
/// node in a managed workspace document.
[[nodiscard]] const AstNode &
resolve_ast_node(const Documents &documents,
                 const AstNodeDescription &description) noexcept;

/// Resolves a symbol description to the current in-memory AST node, reusing
/// `currentDocument` for same-document lookups.
[[nodiscard]] const AstNode &
resolve_ast_node(const Documents &documents,
                 const AstNodeDescription &description,
                 const Document &currentDocument) noexcept;

} // namespace workspace
} // namespace pegium
