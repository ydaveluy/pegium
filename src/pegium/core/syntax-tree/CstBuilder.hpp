#pragma once

#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <string_view>
#include <vector>

#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium {

/// Incremental builder for one `RootCstNode`.
///
/// The parser uses `CstBuilder` as a streaming recorder:
/// - `enter()` opens a non-leaf node
/// - `leaf()` appends a leaf node at the current depth
/// - `exit()` finalizes the currently open node and links it to its parent
///
/// The builder supports speculative parsing through `mark()` / `rewind()`
/// without a separate finalize pass.
class CstBuilder {
public:
  using Iterator = RootCstNode::Iterator;
  using StackIndex = std::uint32_t;

  static_assert(std::numeric_limits<StackIndex>::max() >= kMaxNodeCount,
                "StackIndex must represent full CST depth without truncation");

  /// Sibling linkage state for one nesting depth during streaming build.
  struct Frame {
    NodeId parent; // node whose children are being built at this
                   // depth (kNoNode for root-level)
    NodeId last;   // last child already linked at this depth (or kNoNode)
  };

  /// Snapshot of the builder state suitable for speculative rewind.
  struct Checkpoint {
    NodeCount nodeCount;
    NodeId current;
    StackIndex depth;
    NodeId lastAtLevel;   // snapshot of frames[depth].last
    NodeId parentAtLevel; // snapshot of frames[depth].parent
  };

  explicit CstBuilder(RootCstNode &root) : _root(root) {
    _frames.resize(kInitialStackCapacity);
    _frames[0].parent = kNoNode;
    _frames[0].last = kNoNode;
  }

  /// Clears the current CST content and resets the builder to its initial state.
  void reset() noexcept {
    _root._nodeCount = 0;
    _current = kNoNode;
    _depth = 0;
    _frames[0].parent = kNoNode;
    _frames[0].last = kNoNode;
  }

  /// Returns a checkpoint that can later be passed to `rewind(...)`.
  [[nodiscard]] Checkpoint mark() const noexcept {
    assert(_depth < _frames.size());
    return {
        .nodeCount = _root._nodeCount,
        .current = _current,
        .depth = _depth,
        .lastAtLevel = _frames[_depth].last,
        .parentAtLevel = _frames[_depth].parent,
    };
  }

  /// Restores the CST builder state to the given checkpoint.
  ///
  /// If no node was created since `mark()`, this function is a no-op.
  inline void rewind(const Checkpoint &cp) noexcept {
    if (_root._nodeCount == cp.nodeCount)
      return;
    _root._nodeCount = cp.nodeCount;
    _current = cp.current;
    _depth = cp.depth;

    // Cut the sibling link that may have been created after mark()
    if (cp.lastAtLevel != kNoNode) {
      // lastAtLevel existed at mark() time, so it must still be valid after
      // rewind
      assert(static_cast<NodeCount>(cp.lastAtLevel) < cp.nodeCount);
      _root.node(cp.lastAtLevel).nextSiblingId = kNoNode;
    }

    assert(_depth < _frames.size());
    _frames[_depth].parent = cp.parentAtLevel;
    _frames[_depth].last = cp.lastAtLevel;
  }

  // --------------------------------------------------------------------------
  // Streaming build: nextSiblingId is produced without a finalize pass.
  //
  // Policy:
  // - leaf(): node is complete -> link immediately at current depth
  // - enter(): alloc open node, descend, but DO NOT link yet
  // - exit(): finalize node, then link it at parent depth
  // --------------------------------------------------------------------------

  /// Opens a new non-leaf node at the current cursor position.
  ///
  /// The node remains incomplete until the matching `exit(...)`.
  inline void enter() noexcept {
    const NodeId id = _root.alloc_node_uninitialized();
    _current = id;

    // Descend: set up next depth for children of this node
    ++_depth;
    if (_depth < _frames.size()) [[likely]] {
      _frames[_depth] = {id, kNoNode};
    } else {
      _frames.emplace_back(id, kNoNode);
    }
  }

  /// Finalizes the node opened by the last unmatched `enter()`.
  ///
  /// `beginOffset` and `endOffset` describe the full covered source span of the
  /// node.
  inline void exit(TextOffset beginOffset, TextOffset endOffset,
                   const grammar::AbstractElement *ge) noexcept {
    assert(ge);
    assert(_current != kNoNode);
    assert(_depth > 0); // must have a parent depth to link into

    const bool hasChildren = _frames[_depth].last != kNoNode;

    // Finalize node created by enter()
    _root.node(_current) = {
        .begin = beginOffset,
        .end = endOffset,
        .grammarElement = ge,
        .nextSiblingId = kNoNode,
        .isLeaf = !hasChildren,
        .isHidden = false,
        .isRecovered = false,
    };

    // Pop to parent depth
    --_depth;

    // Link current among siblings at parent depth
    NodeId &last = _frames[_depth].last;
    if (last != kNoNode) [[likely]] {
      _root.node(last).nextSiblingId = _current;
    }
    last = _current;

    // Restore current to parent node at this depth
    _current = _frames[_depth].parent;
  }

  inline void leaf(TextOffset beginOffset, TextOffset endOffset,
                   const grammar::AbstractElement *ge, bool hidden = false,
                   bool recovered = false) noexcept {
    assert(ge);
    assert(_depth < _frames.size());

    const NodeId id = _root.alloc_node_uninitialized();
    _root.node(id) = {
        .begin = beginOffset,
        .end = endOffset,
        .grammarElement = ge,
        .nextSiblingId = kNoNode,
        .isLeaf = true,
        .isHidden = hidden,
        .isRecovered = recovered,
    };

    // Link leaf among siblings at current depth
    NodeId &last = _frames[_depth].last;
    if (last != kNoNode) [[likely]] {
      _root.node(last).nextSiblingId = id;
    }
    last = id;
  }

  // --------------------------------------------------------------------------

  /// Returns the number of currently committed CST nodes.
  [[nodiscard]] NodeCount node_count() const noexcept {
    return _root._nodeCount;
  }

  /// Rebinds an already built node to another grammar element.
  ///
  /// This is mainly used by parser helpers that refine the semantic element
  /// attached to a previously emitted CST node.
  void override_grammar_element(NodeId id,
                                const grammar::AbstractElement *ge) noexcept {
    assert(id < _root._nodeCount);
    assert(ge);
    _root.node(id).grammarElement = ge;
  }

  /// Returns a pointer to the beginning of the input text.
  [[nodiscard]] const char *input_begin() const noexcept {
    const auto text = _root.getText();
    return text.data();
  }
  /// Returns a pointer one past the end of the input text.
  [[nodiscard]] const char *input_end() const noexcept {
    const auto text = _root.getText();
    return text.data() + text.size();
  }
  /// Returns the full source text currently attached to the builder root.
  [[nodiscard]] std::string_view getText() const noexcept {
    return _root.getText();
  }

  /// Returns the owning CST root.
  [[nodiscard]]  RootCstNode *getRootCstNode() noexcept {
    return &_root;
  }

private:
  static constexpr std::size_t kInitialStackCapacity = 128;

  RootCstNode &_root;
  std::vector<Frame> _frames;
  StackIndex _depth = 0;
  NodeId _current = kNoNode;
};

} // namespace pegium
