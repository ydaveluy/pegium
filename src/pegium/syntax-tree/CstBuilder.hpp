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
    std::uint32_t nodeCount;
    NodeId current;
    std::uint32_t stackTop;
  };

  explicit CstBuilder(
      std::string_view text,
      std::pmr::memory_resource *upstream = std::pmr::get_default_resource())
      : _root(std::make_shared<RootCstNode>(std::string{text}, upstream)) {
    // pre-allocate a reasonable amount of stack space
    _stack.resize(128);
    _stack[0] = kNoNode;
  }

  [[nodiscard]] Checkpoint mark() const noexcept {
    return Checkpoint{.nodeCount =
                          static_cast<std::uint32_t>(_root->_nodeCount),
                      .current = _current,
                      .stackTop = _stackTop};
  }

  inline void rewind(const Checkpoint &checkpoint) noexcept {
    _root->_nodeCount = checkpoint.nodeCount;
    _current = checkpoint.current;
    _stackTop = checkpoint.stackTop;
  }

  void reset() noexcept {
    _root->_nodeCount = 0;
    _current = kNoNode;
    _stackTop = 1;
    _linksFinalized = false;
  }

  inline void enter() {
    assert(!_linksFinalized);
    _current = _root->alloc_node_uninitialized();

    if (_stackTop < _stack.size()) [[likely]] {
      _stack[_stackTop] = _current;
    } else {
      _stack.push_back(_current);
    }
    ++_stackTop;
  }

  inline void exit(const char *beginPtr, const char *endPtr,
                   const grammar::AbstractElement *ge) noexcept {
    assert(!_linksFinalized);
    assert(_stackTop > 1);
    assert(_current != kNoNode);
    assert(ge);
    assert(_root->_nodeCount > static_cast<std::uint64_t>(_current) + 1 &&
           "CstNodeBuilder::exit expects at least one child node");


    _root->node(_current) = {
        .begin = _root->offset_of(beginPtr),
        .end = _root->offset_of(endPtr),
        .grammarElement = ge,
        .nextSiblingId = _current= _stack[--_stackTop - 1],
        .isLeaf = false,
        .isHidden = false,
        .isRecovered = false,
    };

  }

  inline void leaf(const char *beginPtr, const char *endPtr,
                   const grammar::AbstractElement *ge, bool hidden = false,
                   bool recovered = false) {
    assert(!_linksFinalized);
    assert(ge);
    const auto id = _root->alloc_node_uninitialized();
    _root->node(id) = {
        .begin = _root->offset_of(beginPtr),
        .end = _root->offset_of(endPtr),
        .grammarElement = ge,
        .nextSiblingId = _current,
        .isLeaf = true,
        .isHidden = hidden,
        .isRecovered = recovered,
    };
  }

  [[nodiscard]] std::uint64_t node_count() const noexcept {
    return _root->_nodeCount;
  }
  void override_grammar_element(NodeId id,
                                const grammar::AbstractElement *ge) noexcept {
    assert(!_linksFinalized);
    assert(id < _root->_nodeCount);
    assert(ge);
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
  std::uint32_t _stackTop = 1;
  NodeId _current = kNoNode;
  bool _linksFinalized = false;
};

} // namespace pegium
