#include <pegium/validation/DiagnosticRanges.hpp>

#include <pegium/syntax-tree/CstUtils.hpp>

namespace pegium::validation {

std::pair<TextOffset, TextOffset> range_of(const AstNode &node) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }
  return range_of(node.getCstNode());
}

std::pair<TextOffset, TextOffset> range_of(const CstNodeView &node) noexcept {
  if (!node) {
    return {0u, 0u};
  }
  return {node.getBegin(), node.getEnd()};
}

std::pair<TextOffset, TextOffset>
range_for_feature(const AstNode &node, std::string_view feature) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }

  if (auto featureNode = find_node_for_feature(node.getCstNode(), feature);
      featureNode.has_value()) {
    return range_of(*featureNode);
  }

  return range_of(node);
}

std::pair<TextOffset, TextOffset>
range_for_feature(const AstNode &node, std::string_view feature,
                  std::size_t index) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }

  const auto matches = find_nodes_for_feature(node.getCstNode(), feature);
  if (index < matches.size()) {
    return range_of(matches[index]);
  }

  return range_of(node);
}

std::pair<TextOffset, TextOffset>
range_for_name_like(const AstNode &node, std::string_view name) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }

  if (auto featureNode = find_node_for_feature(node.getCstNode(), "name");
      featureNode.has_value()) {
    return range_of(*featureNode);
  }

  if (auto nameNode = find_name_like_node(node.getCstNode(), name);
      nameNode.has_value()) {
    return range_of(*nameNode);
  }

  return range_of(node);
}

} // namespace pegium::validation
