#pragma once

#include <cstddef>
#include <iterator>
#include <string_view>

#include <pegium/syntax-tree/RootCstNode.hpp>

namespace pegium {

class CstNodeView;

// ---------------- Child iterator ----------------

class ChildIterator {
public:
  using iterator_category = std::forward_iterator_tag;
  using value_type = CstNodeView;
  using difference_type = std::ptrdiff_t;
  using reference = value_type;
  using pointer = void;

  ChildIterator() = default;
  ChildIterator(const RootCstNode *root, NodeId cur) noexcept
      : _root(root), _cur(cur) {}

  reference operator*() const noexcept;
  ChildIterator &operator++() noexcept;

  friend bool operator==(const ChildIterator &a,
                         const ChildIterator &b) noexcept = default;

private:
  const RootCstNode *_root = nullptr;
  NodeId _cur = kNoNode;
};

// ---------------- CstNodeView ----------------

class CstNodeView {
public:
  CstNodeView() = default;
  CstNodeView(const RootCstNode *root, NodeId id) noexcept
      : _root(root), _id(id) {}

  std::string_view getText() const noexcept {
    const auto &n = _root->node(_id);
    return std::string_view(_root->_text.data() + n.begin, n.end - n.begin);
  }
  TextOffset getBegin() const noexcept { return _root->node(_id).begin; }
  TextOffset getEnd() const noexcept { return _root->node(_id).end; }
  bool isHidden() const noexcept { return _root->node(_id).isHidden; }
  bool isLeaf() const noexcept { return _root->node(_id).isLeaf; }
  bool isRecovered() const noexcept { return _root->node(_id).isRecovered; }

  const grammar::AbstractElement *getGrammarElement() const noexcept {
    return _root->node(_id).grammarElement;
  }

  ChildIterator begin() const noexcept {
    if (_root->node(_id).isLeaf) {
      return ChildIterator(_root, kNoNode);
    }
    return ChildIterator(_root, _id + 1);
  }

  ChildIterator end() const noexcept { return ChildIterator(_root, kNoNode); }

private:
  const RootCstNode *_root = nullptr;
  NodeId _id = kNoNode;
};

inline ChildIterator RootCstNode::begin() const noexcept {
  return ChildIterator(this, _nodeCount == 0 ? kNoNode : 0u);
}

inline ChildIterator RootCstNode::end() const noexcept {
  return ChildIterator(this, kNoNode);
}

inline ChildIterator::reference ChildIterator::operator*() const noexcept {
  return CstNodeView(_root, _cur);
}

inline ChildIterator &ChildIterator::operator++() noexcept {
  _cur = (_cur == kNoNode) ? kNoNode : _root->node(_cur).nextSiblingId;
  return *this;
}

} // namespace pegium
