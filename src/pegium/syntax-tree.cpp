
#include <algorithm>
#include <ostream>
#include <pegium/syntax-tree.hpp>
namespace pegium {

std::ostream &operator<<(std::ostream &os, const CstNode &obj) {
  os << "{\n";
  if (obj.grammarSource)
    os << "\"grammarSource\": \"" << *obj.grammarSource << "\",\n";

  if (obj.isLeaf()) {
    os << "\"text\": \"" << obj.text << "\",\n";
  }

  if (obj.hidden) {
    os << "\"hidden\": true,\n";
  }
  if (!obj.content.empty()) {

    os << "\"content\": [\n";
    for (const auto &n : obj.content) {
      os << n;
    }
    os << "],\n";
  }

  os << "},\n";
  return os;
}

const AstNode *AstNode::getContainer() const noexcept { return _container; }

AstNode *AstNode::getContainer() noexcept { return _container; }

void AstNode::setContainer(AstNode *container, std::any property,
                           std::size_t index) {
  if (_container) {
    std::erase(_container->_content, this);
  }
  _container = container;
  _containerProperty = std::move(property);
  _containerIndex = index;
  _container->_content.push_back(this);
}

} // namespace pegium