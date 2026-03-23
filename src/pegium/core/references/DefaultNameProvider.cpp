#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/references/DefaultNameProvider.hpp>

#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <cassert>
#include <string>
#include <string_view>
#include <type_traits>

namespace pegium::references {

std::optional<std::string>
DefaultNameProvider::getName(const AstNode &node) const noexcept {
  if (const auto *namedNode = dynamic_cast<const NamedAstNode *>(&node);
      namedNode != nullptr) {
    if (namedNode->name.empty()) {
      return std::nullopt;
    }
    return namedNode->name;
  }

  const auto nameNode = getNameNode(node);
  if (!nameNode.has_value()) {
    return std::nullopt;
  }

  const auto *grammarElement = nameNode->getGrammarElement();
  assert(grammarElement != nullptr);
  assert(grammarElement->getKind() == grammar::ElementKind::Assignment);
  const auto &assignment =
      *static_cast<const grammar::Assignment *>(grammarElement);
  const auto value = assignment.getValue(&node);
  if (!value.isRuleValue()) {
    return std::nullopt;
  }
  auto name = std::visit(
      []<typename T>(const T &item) -> std::string {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Value, std::string>) {
          return item;
        } else if constexpr (std::same_as<Value, std::string_view>) {
          return std::string(item);
        } else {
          return {};
        }
      },
      value.ruleValue());
  if (name.empty()) {
    return std::nullopt;
  }
  return name;
}

std::optional<CstNodeView>
DefaultNameProvider::getNameNode(const AstNode &node) const noexcept {
  if (!node.hasCstNode()) {
    return std::nullopt;
  }
  return find_node_for_feature(node.getCstNode(), "name");
}

} // namespace pegium::references
