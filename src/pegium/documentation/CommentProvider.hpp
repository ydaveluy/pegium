#pragma once

#include <string_view>
#include <pegium/syntax-tree/AstNode.hpp>


namespace pegium::documentation {

class CommentProvider {
public:
  virtual ~CommentProvider() = default;
  virtual std::string_view getComment(const AstNode &node) const noexcept = 0;
};

} // namespace pegium::documentation
