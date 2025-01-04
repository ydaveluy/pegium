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
  DataTypeRule(std::string_view name, std::string_view description = "")
      : AbstractRule{name, description} {

    if constexpr (std::is_same_v<T, std::string>) {
      _value_converter = [](const CstNode &node) {
        std::string value;
        value.reserve(node.text.size());

        for (auto it = node.begin(); it != node.end(); ++it) {
          assert(it->grammarSource);
          // std::cout << n << std::endl;
          if (!it->hidden) {
            switch (it->grammarSource->getKind()) {
            case GrammarElementKind::DataTypeRule:
            case GrammarElementKind::TerminalRule:
            case GrammarElementKind::ParserRule:
              value += std::any_cast<std::string>(
                  static_cast<const IRule *>(it->grammarSource)
                      ->getAnyValue(*it));
              it.prune();
              break;
            default:
              assert(it->isLeaf());
              // if (it->isLeaf())
              value += it->text;
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
    // if (_value_converter)
    return _value_converter(node);
    // throw std::logic_error("value converter not provided.");
  }
  std::any getAnyValue(const CstNode &node) const override {
    return getValue(node);
  }
  pegium::GenericParseResult parseGeneric(
      std::string_view text,
      std::unique_ptr<pegium::grammar::IContext> context) const override {
    auto result = parse(text, std::move(context));
    return {.root_node = result.root_node};
  }
  pegium::ParseResult<T>
  parse(std::string_view text,
        std::unique_ptr<pegium::grammar::IContext> context) const {
    // assert(_current);
    pegium::ParseResult<T> result;
    result.root_node = std::make_shared<RootCstNode>();
    result.root_node->fullText = text;
    std::string_view sv = result.root_node->fullText;
    result.root_node->text = result.root_node->fullText;
    // result.root_node->grammarSource = element;
    // auto c = _parser->createContext();

    // skip leading hidden nodes
    auto i = context->skipHiddenNodes(sv, *result.root_node);

    result.len = i + parse_rule({sv.data() + i, sv.size() - i},
                                *result.root_node, *context);

    // std::cout << *result.root_node << std::endl;
    result.ret = result.len == sv.size();
    if (result.ret)
      result.value = getValue(*result.root_node);

    return result;
  }
  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const override {
    assert(element && "The rule definition is missing !");
    auto size = parent.content.size();
    auto &node = parent.content.emplace_back();
    auto i = element->parse_rule(sv, node, c);
    if (fail(i)) {
      parent.content.resize(size);
      return PARSE_ERROR;
    }
    node.text = {sv.data(), i};
    node.grammarSource = this;

    return i;
  }

  using AbstractRule::operator=;

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::DataTypeRule;
  }

private:
  std::function<T(const CstNode &)> _value_converter;
};

} // namespace pegium::grammar