#pragma once
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/IParser.hpp>
#include <string>
#include <string_view>
namespace pegium::parser {

template <typename T = std::string>
  requires(!std::derived_from<T, AstNode>)
struct DataTypeRule final : AbstractRule {
  using type = T;

  using AbstractRule::AbstractRule;
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::DataTypeRule;
  }

  T getValue(const CstNode &node) const { return _value_converter(node); }
  std::any getAnyValue(const CstNode &node) const override {
    return getValue(node);
  }
  GenericParseResult parseGeneric(std::string_view text,
                                  std::unique_ptr<IContext> context) const {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  ParseResult<T> parse(std::string_view text,
                       std::unique_ptr<IContext> context) const {
    ParseResult<T> result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->text = result.root_node->fullText;

    // skip leading hidden nodes
    auto i = context->skipHiddenNodes(sv, *result.root_node);

    auto match = parse_rule({i.offset, sv.end()}, *result.root_node, *context);

    // std::cout << *result.root_node << std::endl;
    result.ret = match.valid;
    if (result.ret)
      result.value = getValue(*result.root_node);

    return result;
  }
  MatchResult parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const {
    assert(element && "The rule definition is missing !");
    CstNode node;
    auto i = element->parse_rule(sv, node, c);
    if (i) {
      node.text = {sv.begin(), i.offset};
      node.grammarSource = this;
      parent.content.emplace_back(std::move(node));
    }
    return i;
  }

  using AbstractRule::operator=;

private:
  std::function<T(const CstNode &)> _value_converter =
      initializeDefaultValueConverter();

  std::function<T(const CstNode &)> initializeDefaultValueConverter() {
    if constexpr (std::is_same_v<T, std::string>) {
      return [](const CstNode &node) {
        std::string value;
        for (const auto &it : node.content) {
          assert(it.grammarSource);
          // std::cout << n << std::endl;
          if (!it.hidden) {
            switch (it.grammarSource->getKind()) {
            case ElementKind::TerminalRule: {
              auto any = static_cast<const grammar::Rule *>(it.grammarSource)
                             ->getAnyValue(it);
              if (auto ptr = std::any_cast<std::string_view>(&any)) {
                value += *ptr;
              } else if (auto ptr = std::any_cast<std::string>(&any)) {
                value += *ptr;
              } else {
                value += it.text;
              }
              break;
            }
            case ElementKind::DataTypeRule:
            case ElementKind::ParserRule: {
              auto any = static_cast<const grammar::Rule *>(it.grammarSource)
                             ->getAnyValue(it);
              if (auto ptr = std::any_cast<std::string>(&any)) {
                value += *ptr;
              } else {
                value += it.text;
              }
              break;
            }
            default:
              assert(it.isLeaf());
              value += it.text;
              break;
            }
          }
        }

        return value;
      };
      return [this](const CstNode &node) -> T {
        throw std::logic_error("ValueConvert not provided for rule " +
                               getName());
      };
    }
  }
};

} // namespace pegium::parser