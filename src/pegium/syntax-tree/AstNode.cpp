#include <pegium/syntax-tree/AstNode.hpp>

#include <algorithm>

namespace pegium {

const AstNode *AstNode::getContainer() const noexcept { return _container; }

AstNode *AstNode::getContainer() noexcept { return _container; }

void AstNode::setContainer(AstNode *container, std::any property,
                           std::size_t index) {
  if (_container) {
    auto &content = _container->_content;
    auto it = std::ranges::find(content, this);
    if (it != content.end()) {
      content.erase(it);
    }
  }
  _container = container;
  _containerProperty = std::move(property);
  _containerIndex = index;
  _container->_content.push_back(this);
}

} // namespace pegium
