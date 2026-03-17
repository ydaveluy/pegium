#pragma once

#include <string>
#include <string_view>

#include <pegium/syntax-tree/AstNode.hpp>

namespace pegium::workspace {

class AstNodeLocator {
public:
  static std::string getAstNodePath(const AstNode &node);

  static const AstNode *getAstNode(const AstNode &root, std::string_view path);
  static AstNode *getAstNode(AstNode &root, std::string_view path);
};

} // namespace pegium::workspace
