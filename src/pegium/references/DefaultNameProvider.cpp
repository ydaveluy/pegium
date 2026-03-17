#include <pegium/grammar/Assignment.hpp>
#include <pegium/references/DefaultNameProvider.hpp>

#include <pegium/grammar/FeatureValue.hpp>

#include <string>
#include <string_view>
#include <type_traits>

namespace pegium::references {

std::string DefaultNameProvider::getName(const AstNode &node) const noexcept {
  auto read_name = [](const grammar::FeatureValue &value) -> std::string {
    if (!value.isRuleValue()) {
      return {};
    }
    const auto &ruleValue = value.ruleValue();
    return std::visit(
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
        ruleValue);
  };

  auto cst = getNameNode(node);
  if (!cst) {
    return {};
  }

  const auto *assignment =
      static_cast<const grammar::Assignment *>(cst.getGrammarElement());

  const auto value = assignment->getValue(&node);
  if (const auto name = read_name(value); !name.empty()) {
    return name;
  }
  return std::string(cst.getText());
}

CstNodeView
DefaultNameProvider::getNameNode(const AstNode &node) const noexcept {
  auto cst = node.getCstNode();
  if (!cst) {
    return {};
  }

  for (auto child : cst) {
    if (child.getGrammarElement()->getKind() !=
        grammar::ElementKind::Assignment) {
      continue;
    }
    const auto *assignment =
        static_cast<const grammar::Assignment *>(child.getGrammarElement());
    if (assignment->getFeature() == "name") {
      return child;
    }
  }

  return {};
}

} // namespace pegium::references
