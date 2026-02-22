#pragma once

#include <optional>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::parser::detail {

inline std::optional<CstNodeView>
findFirstMatchingNode(const CstNodeView &node,
                      const grammar::AbstractElement *element) {
  if (!node.isHidden() && node.getGrammarElement() == element) {
    return node;
  }
  for (const auto &child : node) {
    if (auto found = findFirstMatchingNode(child, element); found.has_value()) {
      return found;
    }
  }
  return std::nullopt;
}

inline std::optional<CstNodeView>
findFirstMatchingNode(const RootCstNode &root,
                      const grammar::AbstractElement *element) {
  for (const auto &child : root) {
    if (auto found = findFirstMatchingNode(child, element); found.has_value()) {
      return found;
    }
  }
  return std::nullopt;
}

inline std::optional<CstNodeView>
findFirstRootMatchingNode(const RootCstNode &root,
                          const grammar::AbstractElement *element) {
  for (const auto &child : root) {
    if (!child.isHidden() && child.getGrammarElement() == element) {
      return child;
    }
  }
  return std::nullopt;
}

inline std::optional<CstNodeView>
firstVisibleChild(const CstNodeView &node) {
  for (const auto &child : node) {
    if (!child.isHidden()) {
      return child;
    }
  }
  return std::nullopt;
}

} // namespace pegium::parser::detail
