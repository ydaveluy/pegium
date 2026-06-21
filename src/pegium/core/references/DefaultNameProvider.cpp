#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/references/DefaultNameProvider.hpp>

#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

#include <cassert>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium::references {

AstNodeName DefaultNameProvider::nameOf(const AstNode &node) const {
  // The CST source is always reported when present, independently of the
  // name validity: `getNameNode()` callers (LSP rename/highlight) need the
  // location of the `name` assignment even when the produced value is
  // empty or non-string. `nameOf` callers gate behaviour on `info.empty()`
  // when they need both pieces.
  AstNodeName info;
  if (node.hasCstNode()) {
    if (auto cst = find_node_for_feature(node.getCstNode(), "name");
        cst.has_value()) {
      info.cstNode = *cst;
    }
  }

  // Fast path: `NamedAstNode` exposes the name directly.
  if (const auto *namedNode = dynamic_cast<const NamedAstNode *>(&node);
      namedNode != nullptr) {
    if (!namedNode->name.empty()) {
      info.name = namedNode->name;
    }
    return info;
  }

  // Fallback: read the `name` feature value through the grammar.
  if (!info.cstNode.valid()) {
    return info;
  }
  const auto *grammarElement = info.cstNode.getGrammarElement();
  assert(grammarElement != nullptr);
  assert(grammarElement->getKind() == grammar::ElementKind::Assignment);
  const auto &assignment =
      *static_cast<const grammar::Assignment *>(grammarElement);
  const auto value = assignment.getValue(&node);
  if (!value.isRuleValue()) {
    return info;
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
  if (!name.empty()) {
    info.name = std::move(name);
  }
  return info;
}

} // namespace pegium::references
