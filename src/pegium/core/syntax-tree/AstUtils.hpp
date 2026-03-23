#pragma once

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {

namespace workspace {
struct Document;
}

/// Returns the document that owns `node`, or `nullptr` when none is available.
///
/// A document can be recovered only for AST nodes that still carry a valid CST
/// node bound to a `RootCstNode`.
[[nodiscard]] const workspace::Document *
tryGetDocument(const AstNode &node) noexcept;

/// Returns the owning document of `node`.
///
/// Throws `std::logic_error` when `node` is not currently attached to a CST
/// backed by a document.
[[nodiscard]] const workspace::Document &getDocument(const AstNode &node);

/// Returns the deepest AST node whose CST range contains `offset`.
///
/// The search walks descendants depth-first and returns `nullptr` when `node`
/// has no CST node or when `offset` falls outside `node`'s CST range.
[[nodiscard]] const AstNode *find_ast_node_at_offset(const AstNode &node,
                                                     TextOffset offset);

} // namespace pegium
