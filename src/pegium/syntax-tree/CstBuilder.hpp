#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium {

class CstBuilder {
public:
  using Iterator = RootCstNode::Iterator;
  struct Checkpoint {
    std::uint64_t nodeCount = 0;
    NodeId current = kNoNode;
    std::size_t stackTop = 0;
    const char *cursor = nullptr;
  };

  explicit CstBuilder(
      std::string_view text,
      std::pmr::memory_resource *upstream = std::pmr::get_default_resource())
      : _root(std::make_shared<RootCstNode>(std::string{text}, upstream)) {}

  [[nodiscard]] Checkpoint mark(const char *cursor) const noexcept {
    return Checkpoint{
        .nodeCount = _root->_nodeCount,
        .current = _current,
        .stackTop = _stackTop,
        .cursor = cursor,
    };
  }

  [[nodiscard]] const char *rewind(const Checkpoint &checkpoint) noexcept {
    _root->_nodeCount = checkpoint.nodeCount;
    _current = checkpoint.current;
    _stackTop = checkpoint.stackTop;
    return checkpoint.cursor;
  }

  void reset() noexcept {
    _root->_nodeCount = 0;
    _current = kNoNode;
    _stackTop = 0;
    _linksFinalized = false;
  }

  void enter(const char *beginPtr) {
    assert(!_linksFinalized);
    const NodeId parent = _current;
    const NodeId id = _root->alloc_node_uninitialized();
    if (_stackTop == _stack.size()) [[unlikely]] {
      _stack.push_back(id);
    } else [[likely]] {
      _stack[_stackTop] = id;
    }
    ++_stackTop;
    _current = id;
    CstNode &n = _root->node(id);
    n.begin = _root->offset_of(beginPtr);
    n.end = n.begin;
    n.grammarElement = nullptr;
    n.nextSiblingId = parent;
    n.isLeaf = true;
    n.isHidden = false;
    n.isRecovered = false;
  }

  void exit(const char *endPtr, const grammar::AbstractElement *ge) noexcept {
    assert(!_linksFinalized);
    assert(_stackTop > 0);
    assert(_current != kNoNode);

    const NodeId id = _current;
    CstNode &n = _root->node(id);

    assert(_root->_nodeCount > static_cast<std::uint64_t>(id) + 1 &&
           "CstNodeBuilder::exit expects at least one child node");

    n.isLeaf = false;
    n.end = _root->offset_of(endPtr);
    n.grammarElement = ge;
    n.isHidden = false;

    --_stackTop;
    NodeId parent;
    if (_stackTop > 0) [[likely]] {
      parent = _stack[_stackTop - 1];
    } else [[unlikely]] {
      parent = kNoNode;
    }
    _current = n.nextSiblingId = parent;
  }

  void leaf(const char *beginPtr, const char *endPtr,
            const grammar::AbstractElement *ge, bool hidden = false,
            bool recovered = false) {
    assert(!_linksFinalized);
    const TextOffset beginOff = _root->offset_of(beginPtr);
    const TextOffset endOff = _root->offset_of(endPtr);

    const NodeId id = _root->alloc_node_uninitialized();
    CstNode &n = _root->node(id);

    n.begin = beginOff;
    n.end = endOff;
    n.grammarElement = ge;
    n.nextSiblingId = _current;
    n.isLeaf = true;
    n.isHidden = hidden;
    n.isRecovered = recovered;
    if (recovered && _current != kNoNode) {
      mark_recovered_ancestors(_current);
    }
  }

  [[nodiscard]] std::uint64_t node_count() const noexcept {
    return _root->_nodeCount;
  }
  void override_grammar_element(NodeId id,
                                const grammar::AbstractElement *ge) noexcept {
    assert(!_linksFinalized);
    assert(id < _root->_nodeCount);
    _root->node(id).grammarElement = ge;
  }

  [[nodiscard]] const char *input_begin() const noexcept {
    return _root->_text.data();
  }
  [[nodiscard]] const char *input_end() const noexcept {
    return _root->_text.data() + _root->_text.size();
  }
  [[nodiscard]] std::string_view getText() const noexcept {
    return _root->getText();
  }
  [[nodiscard]] std::shared_ptr<RootCstNode> finalize() {
    finalize_links();
    return _root;
  }

private:
  void mark_recovered_ancestors(NodeId parent) noexcept {
    while (parent != kNoNode) {
      CstNode &node = _root->node(parent);
      node.isRecovered = true;
      parent = node.nextSiblingId;
    }
  }

  void finalize_links() {
    if (_linksFinalized) [[unlikely]] {
      return;
    }
    assert(_root->_nodeCount <=
               static_cast<std::uint64_t>(std::numeric_limits<NodeId>::max()) &&
           "Too many CST nodes for NodeId type");

    const auto n = static_cast<NodeId>(_root->_nodeCount);
    if (n == 0) [[unlikely]] {
      _linksFinalized = true;
      return;
    }

    std::vector<NodeId> lastChild(n, kNoNode);
    NodeId rootLast = kNoNode;

    for (NodeId child = 0; child < n; ++child) {
      CstNode &cn = _root->node(child);
      NodeId parent = cn.nextSiblingId;
      cn.nextSiblingId = kNoNode;

      if (parent == kNoNode) {
        if (rootLast != kNoNode) {
          _root->node(rootLast).nextSiblingId = child;
        }
        rootLast = child;
        continue;
      }

      NodeId &last = lastChild[parent];
      if (last != kNoNode) {
        _root->node(last).nextSiblingId = child;
      }
      last = child;
    }

    _linksFinalized = true;
  }

  std::shared_ptr<RootCstNode> _root;
  std::vector<NodeId> _stack;
  std::size_t _stackTop = 0;
  NodeId _current = kNoNode;
  bool _linksFinalized = false;
};

} // namespace pegium
