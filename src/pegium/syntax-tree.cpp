
#include <ostream>
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

std::generator<const AstNode *> AstNode::getContent() const {
  for (const AstNode *child : _content) {
    co_yield child;
  }
}
std::generator<const AstNode *> AstNode::getAllContent() const {
  for (const AstNode *child : _content) {
    co_yield child;
    for (const AstNode *sub : child->getAllContent()) {
      co_yield sub;
    }
  }
}

std::generator<AstNode *> AstNode::getContent() {
  for (AstNode *child : _content) {
    co_yield child;
  }
}
std::generator<AstNode *> AstNode::getAllContent() {
  for (AstNode *child : _content) {
    co_yield child;
    for (AstNode *sub : child->getAllContent()) {
      co_yield sub;
    }
  }
}

const AstNode *AstNode::getContainer() const noexcept { return _container; }

AstNode *AstNode::getContainer() noexcept { return _container; }

void AstNode::setContainer(AstNode *container, std::any property,
                           std::size_t index) {
  if (_container) {
    std::erase(_container->_content, this);
  }
  container->_content.push_back(this);
  _container = container;
  _containerProperty = property;
  _containerIndex = index;
}

} // namespace pegium