#include <pegium/syntax-tree.hpp>

namespace pegium {

CstNode::Iterator::Iterator(CstNode::Iterator::pointer root) {
  if (root)
    stack.emplace_back(root, 0);
}

CstNode::Iterator::reference CstNode::Iterator::operator*() const {
  return *stack.back().first;
}
CstNode::Iterator::pointer CstNode::Iterator::operator->() const {
  return stack.back().first;
}

CstNode::Iterator &CstNode::Iterator::operator++() {
  advance();
  return *this;
}

bool CstNode::Iterator::operator==(const Iterator &other) const {
  return stack/*.empty()*/ == other.stack/*.empty()*/;
}
/*bool CstNode::Iterator::operator!=(const Iterator &other) const {
  return !(*this == other);
}*/

void CstNode::Iterator::prune() {
  pruneCurrent = true;
}

void CstNode::Iterator::advance() {
          while (!stack.empty()) {
                auto [node, index] = stack.back();
                stack.pop_back();

                // Skip the current node's subtree if prune was called
                if (pruneCurrent) {
                    pruneCurrent = false;  // Reset prune flag
                    continue;
                }

                // Traverse child nodes
                if (index < node->content.size()) {
                    stack.emplace_back(node, index + 1);  // Save next child index for the current node
                    stack.emplace_back(&node->content[index], 0);  // Start with the first child
                    return;
                }
            }
}

} // namespace pegium