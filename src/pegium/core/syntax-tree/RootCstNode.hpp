#pragma once

#include <algorithm>
#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <pegium/core/syntax-tree/CstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>
#include <pegium/core/utils/Errors.hpp>
#include <string_view>
#include <utility>
#include <vector>

namespace pegium {

namespace workspace {
struct Document;
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

  explicit RootCstNode(text::TextSnapshot text,
                       std::pmr::memory_resource *upstream =
                           std::pmr::get_default_resource());
  RootCstNode(const RootCstNode &) = delete;
  RootCstNode &operator=(const RootCstNode &) = delete;
  RootCstNode(RootCstNode &&) = delete;
  RootCstNode &operator=(RootCstNode &&) = delete;

  /// Returns the full source text associated with this CST.
  std::string_view getText() const noexcept {
    return _text.view();
  }

  /// Attaches the workspace document that owns this CST in document pipelines.
  void attachDocument(const workspace::Document &document) noexcept {
    _document = std::addressof(document);
  }

  /// Returns whether a workspace document is currently attached.
  [[nodiscard]] bool hasDocument() const noexcept { return _document != nullptr; }

  /// Returns the attached workspace document when available.
  [[nodiscard]] const workspace::Document *tryGetDocument() const noexcept {
    return _document;
  }

  /// Returns the attached workspace document.
  ///
  /// Standalone parses may produce CSTs without any attached document. In that
  /// case, callers should use `hasDocument()` or `tryGetDocument()` first.
  [[nodiscard]] const workspace::Document &getDocument() const {
    if (_document == nullptr) [[unlikely]] {
      throw utils::MissingAstDocumentError(
          "CST root has no attached document.");
    }
    return *_document;
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

  text::TextSnapshot _text;

  std::pmr::monotonic_buffer_resource _pool;
  std::vector<CstNode *> _chunks;

  NodeCount _nodeCount = 0;
  const workspace::Document *_document = nullptr;

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
