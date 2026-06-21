#include <pegium/core/validation/DiagnosticRanges.hpp>

#include <pegium/core/syntax-tree/CstUtils.hpp>

namespace pegium::validation {

std::pair<TextOffset, TextOffset> range_of(const AstNode &node) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }
  const auto cstNode = node.getCstNode();
  return {cstNode.getBegin(), cstNode.getEnd()};
}

std::pair<TextOffset, TextOffset> range_of(const CstNodeView &node) noexcept {
  if (!node) {
    return {0u, 0u};
  }
  return {node.getBegin(), node.getEnd()};
}

std::pair<TextOffset, TextOffset>
range_for_feature(const AstNode &node, std::string_view feature,
                  std::size_t index) noexcept {
  if (!node.hasCstNode()) {
    return {0u, 0u};
  }
  const auto cstNode = node.getCstNode();

  if (auto featureNode = find_node_for_feature(cstNode, feature, index);
      featureNode.has_value()) {
    return range_of(*featureNode);
  }

  return {cstNode.getBegin(), cstNode.getEnd()};
}

} // namespace pegium::validation
