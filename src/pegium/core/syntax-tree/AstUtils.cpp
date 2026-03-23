#include <pegium/core/syntax-tree/AstUtils.hpp>

#include <memory>
#include <stdexcept>

namespace pegium {

const workspace::Document *tryGetDocument(const AstNode &node) noexcept {
  if (node.hasCstNode()) {
    return node.getCstNode().root().tryGetDocument();
  }
  return nullptr;
}

const workspace::Document &getDocument(const AstNode &node) {
  auto *document = tryGetDocument(node);
  if (document == nullptr) {
    throw std::logic_error("AST node has no document.");
  }
  return *document;
}

const AstNode *find_ast_node_at_offset(const AstNode &node, TextOffset offset) {
  if (!node.hasCstNode()) {
    return nullptr;
  }

  if (const auto cstNode = node.getCstNode();
      offset < cstNode.getBegin() || offset > cstNode.getEnd()) {
    return nullptr;
  }

  for (const auto *child : node.getContent()) {
    if (const auto *match = find_ast_node_at_offset(*child, offset);
        match != nullptr) {
      return match;
    }
  }

  return &node;
}

} // namespace pegium
