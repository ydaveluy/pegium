#pragma once

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <pegium/parser/Introspection.hpp>
#include <pegium/syntax-tree/AstFeatures.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium {

/// Returns `true` when `node` references an existing CST node.
[[nodiscard]] bool is_valid(const CstNodeView &node) noexcept;

/// Returns the begin offset of `node`, or `0` when the view is invalid.
[[nodiscard]] TextOffset cst_begin(const CstNodeView &node) noexcept;
/// Returns the end offset of `node`, or `0` when the view is invalid.
[[nodiscard]] TextOffset cst_end(const CstNodeView &node) noexcept;

/// Returns the terminal rule name that produced `node`, when applicable.
///
/// Hidden trivia such as comments are typically backed by terminal rules.
[[nodiscard]] std::optional<std::string_view>
get_terminal_rule_name(const CstNodeView &node) noexcept;

/// Returns the first direct CST child assigned to `feature`.
///
/// This operates on assignment nodes directly under `node`.
[[nodiscard]] std::optional<CstNodeView>
find_node_for_feature(const CstNodeView &node, std::string_view feature);

/// Returns the `index`-th direct CST child assigned to `feature`.
[[nodiscard]] std::optional<CstNodeView>
find_node_for_feature(const CstNodeView &node, std::string_view feature,
                      std::size_t index);

/// Returns all direct CST children assigned to `feature`.
[[nodiscard]] std::vector<CstNodeView>
find_nodes_for_feature(const CstNodeView &node, std::string_view feature);

/// Returns the `index`-th visible CST node that matches `keyword`.
///
/// Literal matching respects the grammar literal's case-sensitivity.
[[nodiscard]] std::optional<CstNodeView>
find_node_for_keyword(const CstNodeView &node, std::string_view keyword,
                      std::size_t index = 0);

/// Returns all visible CST nodes that match `keyword`.
///
/// Literal matching respects the grammar literal's case-sensitivity.
[[nodiscard]] std::vector<CstNodeView>
find_nodes_for_keyword(const CstNodeView &node, std::string_view keyword);

/// Returns the first visible leaf whose text equals `expectedText`.
[[nodiscard]] std::optional<CstNodeView>
find_name_like_node(const CstNodeView &node, std::string_view expectedText);

/// Returns the deepest visible CST node under `node` that contains `offset`.
[[nodiscard]] std::optional<CstNodeView>
find_node_at_offset(const CstNodeView &node, TextOffset offset);

/// Returns the deepest visible top-level CST descendant that contains `offset`.
[[nodiscard]] std::optional<CstNodeView>
find_node_at_offset(const RootCstNode &root, TextOffset offset);

/// Returns the first leaf in allocation order under `root`.
[[nodiscard]] std::optional<CstNodeView>
find_first_leaf(const RootCstNode &root) noexcept;

/// Returns the next leaf in allocation order after `node`.
[[nodiscard]] std::optional<CstNodeView>
find_next_leaf(const CstNodeView &node) noexcept;

/// Returns the CST nodes located strictly between `start` and `end`.
///
/// The search descends into common containers as needed and returns an empty
/// vector when the nodes do not belong to the same root.
[[nodiscard]] std::vector<CstNodeView>
get_interior_nodes(const CstNodeView &start, const CstNodeView &end);

/// Returns the first CST node assigned to AST member `Feature`.
///
/// This is the typed counterpart of the string-based overload above.
template <auto Feature, typename Node>
  requires pegium::detail::AstNodeFeature<Node, Feature>
[[nodiscard]] std::optional<CstNodeView>
find_node_for_feature(const Node &node) {
  if (!node.hasCstNode()) {
    return std::nullopt;
  }
  return find_node_for_feature(node.getCstNode(),
                               parser::detail::member_name_v<Feature>);
}

/// Returns the `index`-th CST node assigned to vector AST member `Feature`.
template <auto Feature, typename Node>
  requires pegium::detail::VectorAstNodeFeature<Node, Feature>
[[nodiscard]] std::optional<CstNodeView>
find_node_for_feature(const Node &node, std::size_t index) {
  if (!node.hasCstNode()) {
    return std::nullopt;
  }
  return find_node_for_feature(node.getCstNode(),
                               parser::detail::member_name_v<Feature>, index);
}

/// Returns all CST nodes assigned to AST member `Feature`.
template <auto Feature, typename Node>
  requires pegium::detail::AstNodeFeature<Node, Feature>
[[nodiscard]] std::vector<CstNodeView>
find_nodes_for_feature(const Node &node) {
  if (!node.hasCstNode()) {
    return {};
  }
  return find_nodes_for_feature(node.getCstNode(),
                                parser::detail::member_name_v<Feature>);
}

} // namespace pegium
