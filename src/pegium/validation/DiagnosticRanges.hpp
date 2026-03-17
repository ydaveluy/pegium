#pragma once

#include <concepts>
#include <optional>
#include <string_view>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/AstFeatures.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::validation {

[[nodiscard]] std::pair<TextOffset, TextOffset>
range_of(const AstNode &node) noexcept;

[[nodiscard]] std::pair<TextOffset, TextOffset>
range_of(const CstNodeView &node) noexcept;

namespace detail {

template <auto Feature, typename Node>
[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_feature_impl(const Node &node,
                       std::optional<std::size_t> index) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }

  std::size_t featureIndex = 0;
  for (const auto child : node.getCstNode()) {
    const auto *grammarElement = child.getGrammarElement();
    if (grammarElement->getKind() != grammar::ElementKind::Assignment) {
      continue;
    }

    const auto *assignment =
        static_cast<const grammar::Assignment *>(grammarElement);
    if (assignment->getFeature() != parser::detail::member_name_v<Feature>) {
      continue;
    }

    if (!index.has_value() || featureIndex == *index) {
      return range_of(child);
    }
    ++featureIndex;
  }

  return range_of(node);
}

} // namespace detail

[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_feature(const AstNode &node, std::string_view feature) noexcept;

[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_feature(const AstNode &node, std::string_view feature,
                  std::size_t index) noexcept;

template <auto Feature, typename Node>
  requires pegium::detail::AstNodeFeature<Node, Feature>
[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_feature(const Node &node) noexcept {
  return detail::range_for_feature_impl<Feature>(node, std::nullopt);
}

template <auto Feature, typename Node>
  requires pegium::detail::VectorAstNodeFeature<Node, Feature>
[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_feature(const Node &node, std::size_t index) noexcept {
  return detail::range_for_feature_impl<Feature>(node, index);
}

[[nodiscard]] std::pair<TextOffset, TextOffset>
range_for_name_like(const AstNode &node, std::string_view name) noexcept;

} // namespace pegium::validation
