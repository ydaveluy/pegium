#pragma once

#include <pegium/core/documentation/CommentProvider.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <string_view>

namespace pegium::documentation {

/// Default comment provider that reads the closest preceding multiline CST comment.
class DefaultCommentProvider : public CommentProvider {
public:
  ~DefaultCommentProvider() override = default;
  /// Returns the nearest preceding multiline comment associated with `node`.
  std::string_view getComment(const AstNode &node) const noexcept override;
};

} // namespace pegium::documentation
