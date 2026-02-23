#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <pegium/grammar/AbstractElement.hpp>

namespace pegium {

using NodeId = std::uint32_t;
inline constexpr NodeId kNoNode = std::numeric_limits<NodeId>::max();
using TextOffset = std::uint32_t;

struct CstNode {
  TextOffset begin;
  TextOffset end;

  // a pointer to the grammar element responsible for this node, cannot be null
  const grammar::AbstractElement *grammarElement;

  // During parsing: temp parent id.
  // After finalize_links(): next sibling id.
  NodeId nextSiblingId;
  // True if this node has no direct child.
  // If false, first child is guaranteed to be (id + 1).
  bool isLeaf;
  bool isHidden;
  bool isRecovered;
};
static_assert(sizeof(CstNode) <= 24,
              "CstNode should be small enough to be efficiently stored in a "
              "vector");
static_assert(std::is_trivially_constructible_v<CstNode>);
static_assert(std::is_trivially_destructible_v<CstNode>);

} // namespace pegium
