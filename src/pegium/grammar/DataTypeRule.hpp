#pragma once
#include <pegium/grammar/AbstractRule.hpp>
#include <string>
#include <string_view>
namespace pegium::grammar {

template <typename T = std::string>
  requires(std::same_as<bool, T> || std::same_as<std::string, T> ||
           std::is_integral_v<T> || std::is_enum_v<T>)
struct DataTypeRule final : AbstractRule {
  using type = T;

  using AbstractRule::AbstractRule;
  DataTypeRule(std::string_view name, std::string_view description = "")
      : AbstractRule{name, description} {

    if constexpr (std::is_same_v<T, std::string>) {
      _value_converter = [this](const CstNode &node) {
        std::string value;
        value.reserve(node.text.size());

        for (auto &it : node.content) {
          assert(it.grammarSource);
          // std::cout << n << std::endl;
          if (!it.hidden) {
            switch (it.grammarSource->getKind()) {
            case GrammarElementKind::TerminalRule: {
              auto any =
                  static_cast<const IRule *>(it.grammarSource)->getAnyValue(it);
              if (auto ptr = std::any_cast<std::string_view>(&any)) {
                value += *ptr;
              } else if (auto ptr = std::any_cast<std::string>(&any)) {
                value += *ptr;
              } else {
                value += it.text;
              }
              break;
            }
            case GrammarElementKind::DataTypeRule:
            case GrammarElementKind::ParserRule: {
              auto any =
                  static_cast<const IRule *>(it.grammarSource)->getAnyValue(it);
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
    }
  }

  DataTypeRule(const DataTypeRule &) = delete;
  DataTypeRule &operator=(const DataTypeRule &) = delete;

  T getValue(const CstNode &node) const {
    assert(_value_converter);
    // TODO protect with try catch and store error in context
    return _value_converter(node);
  }
  std::any getAnyValue(const CstNode &node) const override {
    return getValue(node);
  }
  pegium::GenericParseResult
  parseGeneric(std::string_view text,
               std::unique_ptr<IContext> context) const override {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  pegium::ParseResult<T> parse(std::string_view text,
                               std::unique_ptr<IContext> context) const {
    pegium::ParseResult<T> result;
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
                         IContext &c) const override {
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

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::DataTypeRule;
  }

private:
  std::function<T(const CstNode &)> _value_converter =
      initializeDefaultValueConverter();

  static std::function<T(const CstNode &)> initializeDefaultValueConverter() {
    if constexpr (std::is_same_v<T, std::string>) {
      return [](const CstNode &node) {
        std::string value;
        value.reserve(node.text.size());

        for (auto &it : node.content) {
          assert(it.grammarSource);
          // std::cout << n << std::endl;
          if (!it.hidden) {
            switch (it.grammarSource->getKind()) {
            case GrammarElementKind::TerminalRule: {
              auto any =
                  static_cast<const IRule *>(it.grammarSource)->getAnyValue(it);
              if (auto ptr = std::any_cast<std::string_view>(&any)) {
                value += *ptr;
              } else if (auto ptr = std::any_cast<std::string>(&any)) {
                value += *ptr;
              } else {
                value += it.text;
              }
              break;
            }
            case GrammarElementKind::DataTypeRule:
            case GrammarElementKind::ParserRule: {
              auto any =
                  static_cast<const IRule *>(it.grammarSource)->getAnyValue(it);
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
      return {};
    }
  }
};

} // namespace pegium::grammar