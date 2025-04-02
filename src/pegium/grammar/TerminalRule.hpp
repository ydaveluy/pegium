#pragma once

#include <charconv>
#include <pegium/grammar/AbstractRule.hpp>
#include <string>

namespace pegium::grammar {

template <typename T = std::string> struct TerminalRule final : AbstractRule {
  using type = T;
  constexpr TerminalRule(std::string_view name = "",
                         std::string_view description = "")
      : AbstractRule{name, description} {

    // initialize the value converter for standard types
    if constexpr (std::is_same_v<T, std::string_view>) {
      _value_converter = [](std::string_view sv) { return sv; };
    } else if constexpr (std::is_same_v<T, std::string>) {
      _value_converter = [](std::string_view sv) { return std::string(sv); };
    } else if constexpr (std::is_same_v<T, bool>) {
      _value_converter = [](std::string_view sv) { return sv == "true"; };
    } else if constexpr (std::is_integral_v<T>) {
      _value_converter = [](std::string_view sv) {
        T value;
        auto [ptr, ec] =
            std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc()) {
          throw std::invalid_argument("Conversion failed");
        }
        return value;
      };
    }
  }

  TerminalRule(const TerminalRule &) = delete;
  TerminalRule &operator=(const TerminalRule &) = delete;

  T getValue(const CstNode &node) const {
    assert(_value_converter);
    // TODO protect with try catch and store error in context
    return _value_converter(node.text);
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
    result.root_node->text = result.root_node->fullText;
    std::string_view sv = result.root_node->fullText;
    result.root_node->grammarSource = element;

    result.len = parse_terminal(sv);

    result.value = getValue(*result.root_node);

    result.ret = result.len == sv.size();

    return result;
  }

  std::size_t parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const override {

    assert(element && "The rule definition is missing !");
    auto i = parse_terminal(sv);
    if (fail(i)) {
      return PARSE_ERROR;
    }
    auto &node = parent.content.emplace_back();
    node.text = {sv.data(), i};
    node.grammarSource = this;

    // skip hidden nodes after the token
    return i + c.skipHiddenNodes({sv.data() + i, sv.size() - i}, parent);
  }

  using AbstractRule::operator=;
  void setValueConverter(std::function<T(std::string_view)> &&value_converter) {
    _value_converter = std::move(value_converter);
  }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::TerminalRule;
  }

private:
  std::function<T(std::string_view)> _value_converter;
};
} // namespace pegium::grammar