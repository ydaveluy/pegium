#pragma once

#include <cstdint>
#include <limits>
#include <type_traits>

#include <pegium/core/grammar/AbstractElement.hpp>

namespace pegium {

/// Identifier of one node inside a `RootCstNode`.
///
/// Ids are local to one CST root and follow allocation order.
using NodeId = std::uint32_t;
/// Sentinel used by CST APIs to represent "no node".
inline constexpr NodeId kNoNode = std::numeric_limits<NodeId>::max();
/// Number of nodes stored in one CST root.
using NodeCount = std::uint32_t;
inline constexpr NodeCount kMaxNodeCount = kNoNode;
/// Byte offset in the source text owned by the corresponding document.
using TextOffset = std::uint32_t;
static_assert(std::numeric_limits<TextOffset>::max() ==
              std::numeric_limits<NodeId>::max());

/// Compact storage record for one CST node inside a `RootCstNode`.
///
/// `CstNode` is the raw, allocation-oriented representation used by the CST
/// builder. Most callers should prefer `CstNodeView`, which adds bounds
/// checking, navigation helpers and text access.
///
/// Offsets follow the usual half-open convention: `[begin, end)`.
struct CstNode {
  /// Begin offset in the source text, inclusive.
  TextOffset begin;
  /// End offset in the source text, exclusive.
  TextOffset end;

  /// Grammar element responsible for producing this node.
  ///
  /// This pointer is never null for a valid CST node.
  const grammar::AbstractElement *grammarElement;

  /// Node id of the next sibling in the same parent, or `kNoNode`.
  NodeId nextSiblingId;
  /// True when this node has no direct children.
  ///
  /// When false, the first direct child is guaranteed to be stored at `id + 1`
  /// in the owning root.
  bool isLeaf;
  /// True for hidden nodes such as skipped trivia or comments.
  bool isHidden;
  /// True when the node was introduced or altered by parser recovery.
  bool isRecovered;
};
static_assert(sizeof(CstNode) <= 24,
              "CstNode should be small enough to be efficiently stored in a "
              "vector");
static_assert(std::is_trivially_constructible_v<CstNode>);
static_assert(std::is_trivially_destructible_v<CstNode>);

} // namespace pegium
