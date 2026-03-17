#pragma once

#include <string>

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::references {

class NameProvider {
public:
  virtual ~NameProvider() noexcept = default;
  virtual std::string getName(const AstNode &node) const noexcept = 0;
  virtual CstNodeView getNameNode(const AstNode &node) const noexcept = 0;
};

} // namespace pegium::references
