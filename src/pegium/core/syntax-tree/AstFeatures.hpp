#pragma once

/// Concepts and traits describing AST member features.

#include <concepts>
#include <type_traits>
#include <utility>
#include <vector>

#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium::detail {

template <typename Type> struct IsStdVector : std::false_type {};

template <typename Value, typename Allocator>
struct IsStdVector<std::vector<Value, Allocator>> : std::true_type {};

template <typename Node, auto Feature>
concept AstNodeFeature =
    std::derived_from<Node, AstNode> &&
    requires(const Node &node) { node.*Feature; };

template <typename Node, auto Feature>
using FeatureType =
    std::remove_cvref_t<decltype(std::declval<const Node &>().*Feature)>;

template <typename Node, auto Feature>
concept VectorAstNodeFeature =
    AstNodeFeature<Node, Feature> && IsStdVector<FeatureType<Node, Feature>>::value;

} // namespace pegium::detail
