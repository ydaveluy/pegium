#pragma once

#include <pegium/core/documentation/CommentProvider.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <string>
#include <string_view>

namespace pegium::documentation {

/// Default comment provider that reads the closest preceding multiline CST comment.
class DefaultCommentProvider : public CommentProvider {
public:
  /// Binds the provider to the terminal rule name used for multiline
  /// (documentation) comments. Languages that name that terminal differently
  /// pass their own name; defaults to "ML_COMMENT".
  explicit DefaultCommentProvider(std::string mlCommentRuleName = "ML_COMMENT");
  ~DefaultCommentProvider() override = default;
  /// Returns the nearest preceding multiline comment associated with `node`.
  std::string_view getComment(const AstNode &node) const noexcept override;

private:
  std::string _mlCommentRuleName;
};

} // namespace pegium::documentation
