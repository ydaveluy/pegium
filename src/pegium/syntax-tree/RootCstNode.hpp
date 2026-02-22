#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <pegium/syntax-tree/CstNode.hpp>

namespace pegium {

class ChildIterator;
class CstNodeView;
class CstBuilder;

class RootCstNode {
public:
  using Text = std::string;
  using Iterator = Text::const_iterator;

  explicit RootCstNode(
      Text input = {},
      std::pmr::memory_resource *upstream = std::pmr::get_default_resource())
      : _text(std::move(input)), _pool(upstream), _chunks(upstream) {
    if (_text.size() >
        static_cast<std::size_t>(std::numeric_limits<TextOffset>::max())) {
      throw std::overflow_error(
          "Input text exceeds TextOffset capacity (4 GiB max)");
    }
  }

  constexpr std::string_view getText() const noexcept {
    return std::string_view{_text};
  }

  ChildIterator begin() const noexcept;
  ChildIterator end() const noexcept;

private:
  static constexpr std::uint32_t chunk_size = 4096;
  static constexpr std::uint32_t chunk_shift = std::bit_width(chunk_size) - 1;
  static constexpr std::uint32_t chunk_mask = chunk_size - 1;

  std::string _text;

  std::pmr::monotonic_buffer_resource _pool;
  std::pmr::vector<CstNode *> _chunks;

  std::uint64_t _nodeCount = 0;

  TextOffset offset_of(Iterator it) const noexcept {
    assert(it >= _text.begin() && it <= _text.end());
    return static_cast<TextOffset>(it - _text.begin());
  }
  TextOffset offset_of(const char *ptr) const noexcept {
    assert(ptr >= _text.data() && ptr <= (_text.data() + _text.size()));
    return static_cast<TextOffset>(ptr - _text.data());
  }

  CstNode &node(NodeId id) noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }
  const CstNode &node(NodeId id) const noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }

  CstNode *alloc_chunk_uninitialized() {
    void *mem = _pool.allocate(sizeof(CstNode) * chunk_size, alignof(CstNode));
    return static_cast<CstNode *>(mem);
  }

  NodeId alloc_node_uninitialized() {
    if (_nodeCount >=
        static_cast<std::uint64_t>(std::numeric_limits<NodeId>::max())) {
      throw std::overflow_error("CST node count exceeds NodeId capacity");
    }
    const auto id = static_cast<NodeId>(_nodeCount++);
    const std::uint64_t chunk_index = id >> chunk_shift;

    if (chunk_index == _chunks.size()) {
      _chunks.push_back(alloc_chunk_uninitialized());
    }

    return id;
  }

  friend class CstNodeView;
  friend class ChildIterator;
  friend class CstBuilder;
};

} // namespace pegium
