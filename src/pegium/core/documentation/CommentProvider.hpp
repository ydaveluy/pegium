#pragma once

#include <string_view>
#include <pegium/core/syntax-tree/AstNode.hpp>


namespace pegium::documentation {

/// Extracts the raw comment text associated with one AST node.
class CommentProvider {
public:
  virtual ~CommentProvider() = default;
  /// Returns the source comment associated with `node`, or an empty view when none is available.
  virtual std::string_view getComment(const AstNode &node) const noexcept = 0;
};

} // namespace pegium::documentation
