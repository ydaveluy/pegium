
#include <ostream>
#include <algorithm>
#include <pegium/syntax-tree.hpp>
namespace pegium {

std::ostream &operator<<(std::ostream &os, const CstNode &obj) {
  os << "{\n";
  // if (obj.grammarSource)
  //  os << "\"grammarSource\": \"" << *obj.grammarSource << "\",\n";

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

AstNode::AstNode(AstNode &&other) noexcept { moveFrom(std::move(other)); }

// Move assignment
AstNode &AstNode::operator=(AstNode &&other) noexcept {
  if (this != &other) {
    cleanup();
    moveFrom(std::move(other));
  }
  return *this;
}
AstNode::~AstNode() noexcept { cleanup(); }
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

void AstNode::cleanup() noexcept {
  if (_container) {
    std::erase(_container->_content, this);
    _container = nullptr;
  }
}

void AstNode::moveFrom(AstNode&& other) noexcept {
  _container = other._container;
  _containerProperty = std::move(other._containerProperty);
  _containerIndex = other._containerIndex;
  _node = other._node;

  if (_container) {
    auto& siblings = _container->_content;
    auto it = std::find(siblings.begin(), siblings.end(), &other);
    if (it != siblings.end()) {
      *it = this;
    }
  }

  other._container = nullptr;
}

} // namespace pegium