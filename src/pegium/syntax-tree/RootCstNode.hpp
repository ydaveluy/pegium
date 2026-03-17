#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory>
#include <memory_resource>
#include <pegium/syntax-tree/CstNode.hpp>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace pegium {

namespace workspace {
struct Document;
class TextDocument;
}

class ChildIterator;
class CstNodeView;
class CstBuilder;

/// Owning root of one concrete syntax tree.
///
/// `RootCstNode` stores the full CST in a compact chunked array and keeps a
/// shared ownership reference to the underlying text snapshot so that all
/// `CstNodeView` instances remain able to expose stable `string_view`s.
///
/// Nodes are stored in allocation order, which is also the tree preorder used
/// by the builder:
/// - top-level nodes are iterated by `begin()` / `end()`
/// - non-leaf children are contiguous and the first child is `parent.id() + 1`
/// - sibling chains are linked through `CstNode::nextSiblingId`
class RootCstNode {
public:
  using Text = std::string_view;
  using Iterator = Text::const_iterator;

  /// Builds an empty CST root bound to `document`'s current text snapshot.
  ///
  /// The root keeps the corresponding `TextDocument` alive internally so that
  /// node text slices remain valid for the lifetime of this object.
  explicit RootCstNode(const workspace::Document &document,
                       std::pmr::memory_resource *upstream =
                           std::pmr::get_default_resource());
  RootCstNode(const RootCstNode &) = delete;
  RootCstNode &operator=(const RootCstNode &) = delete;
  RootCstNode(RootCstNode &&) = delete;
  RootCstNode &operator=(RootCstNode &&) = delete;

  /// Returns the full source text associated with this CST.
  constexpr std::string_view getText() const noexcept {
    return _text == nullptr ? std::string_view{}
                            : std::string_view{*_text};
  }

  /// Returns the document from which this CST was produced.
  [[nodiscard]] const workspace::Document &getDocument() const noexcept {
    return _document;
  }

  /// Returns a view over node `id`, or an invalid view when out of bounds.
  [[nodiscard]] CstNodeView get(NodeId id) const noexcept;

  /// Returns an iterator over top-level CST nodes.
  ChildIterator begin() const noexcept;
  /// Returns the end iterator for top-level traversal.
  ChildIterator end() const noexcept;

private:
  static constexpr std::uint32_t chunk_size = 1 << 12;
  static constexpr std::uint32_t chunk_shift = std::bit_width(chunk_size) - 1;
  static constexpr std::uint32_t chunk_mask = chunk_size - 1;
  static_assert(std::has_single_bit(chunk_size),
                "chunk_size must be power of two");

  std::shared_ptr<const workspace::TextDocument> _textOwner;
  const std::string *_text = nullptr;

  std::pmr::monotonic_buffer_resource _pool;
  std::vector<CstNode *> _chunks;

  NodeCount _nodeCount = 0;
  const workspace::Document &_document;

  /// Low-level mutable access to one storage record.
  inline CstNode &node(NodeId id) noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }
  /// Low-level const access to one storage record.
  inline const CstNode &node(NodeId id) const noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }

  /// Reserves one uninitialized node slot and returns its id.
  ///
  /// The caller is responsible for fully initializing the returned record
  /// before exposing it through public APIs.
  inline NodeId alloc_node_uninitialized() {
    static_assert(std::is_trivially_default_constructible_v<CstNode>);
    static_assert(std::is_trivially_destructible_v<CstNode>);
    assert(_nodeCount < kMaxNodeCount &&
           "CST node count exceeds NodeId capacity");

    if (static_cast<std::size_t>(_nodeCount >> chunk_shift) == _chunks.size())
        [[unlikely]] {
      _chunks.push_back(static_cast<CstNode *>(
          _pool.allocate(sizeof(CstNode) * chunk_size, alignof(CstNode))));
    }

    return _nodeCount++;
  }

  friend class CstNodeView;
  friend class ChildIterator;
  friend class CstBuilder;
};

} // namespace pegium
