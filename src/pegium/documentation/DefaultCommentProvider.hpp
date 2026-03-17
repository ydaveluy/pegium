#pragma once

#include <pegium/documentation/CommentProvider.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <string_view>

namespace pegium::documentation {

class DefaultCommentProvider : public CommentProvider {
public:
  ~DefaultCommentProvider() override = default;
  std::string_view getComment(const AstNode &node) const noexcept override;
};

} // namespace pegium::documentation
