#pragma once

#include <string>

#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <pegium/references/NameProvider.hpp>

namespace pegium::references {

class DefaultNameProvider : public NameProvider {
public:
  std::string getName(const AstNode &node) const noexcept override;
  CstNodeView getNameNode(const AstNode &node) const noexcept override;
};

} // namespace pegium::references
