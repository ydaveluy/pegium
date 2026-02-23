#pragma once

#include <bit>
#include <cassert>
#include <cstdint>
#include <limits>
#include <memory_resource>
#include <pegium/syntax-tree/CstNode.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace pegium {

class ChildIterator;
class CstNodeView;
class CstBuilder;

class RootCstNode {
public:
  using Text = std::string;
  using Iterator = Text::const_iterator;

  explicit RootCstNode(Text input = {}, std::pmr::memory_resource *upstream =
                                            std::pmr::get_default_resource())
      : _text(std::move(input)),
        _pool({.max_blocks_per_chunk = 1,
               .largest_required_pool_block = sizeof(CstNode) * chunk_size},
              upstream),
        _chunks(upstream) {
    if (_text.size() >
        static_cast<std::size_t>(std::numeric_limits<TextOffset>::max())) {
      throw std::overflow_error(
          "Input text exceeds TextOffset capacity (4 GiB max)");
    }
  }
  RootCstNode(const RootCstNode &) = delete;
  RootCstNode &operator=(const RootCstNode &) = delete;
  RootCstNode(RootCstNode &&) = delete;
  RootCstNode &operator=(RootCstNode &&) = delete;

  constexpr std::string_view getText() const noexcept {
    return std::string_view{_text};
  }

  ChildIterator begin() const noexcept;
  ChildIterator end() const noexcept;

private:
  static constexpr std::uint32_t chunk_size = 8192;
  static constexpr std::uint32_t chunk_shift = std::bit_width(chunk_size) - 1;
  static constexpr std::uint32_t chunk_mask = chunk_size - 1;

  std::string _text;

  std::pmr::unsynchronized_pool_resource _pool;
  std::pmr::vector<CstNode *> _chunks;

  std::uint64_t _nodeCount = 0;

  inline TextOffset offset_of(Iterator it) const noexcept {
    assert(it >= _text.begin() && it <= _text.end());
    return static_cast<TextOffset>(it - _text.begin());
  }
  inline TextOffset offset_of(const char *ptr) const noexcept {
    assert(ptr >= _text.data() && ptr <= (_text.data() + _text.size()));
    return static_cast<TextOffset>(ptr - _text.data());
  }

  inline CstNode &node(NodeId id) noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }
  inline const CstNode &node(NodeId id) const noexcept {
    return _chunks[id >> chunk_shift][id & chunk_mask];
  }

  inline CstNode *alloc_chunk_uninitialized() {
    void *mem = _pool.allocate(sizeof(CstNode) * chunk_size, alignof(CstNode));
    return static_cast<CstNode *>(mem);
  }

  inline NodeId alloc_node_uninitialized() {
    const auto id = static_cast<NodeId>(_nodeCount++);

    if ((id >> chunk_shift) == _chunks.size()) [[unlikely]] {
      if (static_cast<std::uint64_t>(id) + chunk_size - 1 >
          std::numeric_limits<NodeId>::max()) [[unlikely]] {
        throw std::overflow_error("CST node count exceeds NodeId capacity");
      }
      _chunks.push_back(alloc_chunk_uninitialized());
    }

    return id;
  }

  friend class CstNodeView;
  friend class ChildIterator;
  friend class CstBuilder;
};

} // namespace pegium
