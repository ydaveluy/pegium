#include <pegium/syntax-tree/AstNode.hpp>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace pegium {

void AstNode::setContainer(AstNode &container, std::string_view propertyName,
                           std::size_t index) {
  if (_container == nullptr) {
    attachToContainer(container, propertyName, index);
    return;
  }
  if (_container) {
    auto &content = _container->_content;
    if (auto it = std::ranges::find(content, this); it != content.end()) {
      content.erase(it);
    }
  }
  _container = nullptr;
  attachToContainer(container, propertyName, index);
}

} // namespace pegium
