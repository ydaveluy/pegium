#pragma once

#include <charconv>
#include <pegium/parser/AbstractElement.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/IParser.hpp>
#include <string>

namespace pegium::parser {

template <typename T = std::string> struct TerminalRule final : AbstractRule {
  using type = T;

  using AbstractRule::AbstractRule;
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::TerminalRule;
  }

  T getValue(const CstNode &node) const {
    // TODO protect with try catch and store error in context
    return _value_converter(node.text);
  }

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
    result.root_node->text = result.root_node->fullText;
    std::string_view sv = result.root_node->fullText;
    result.root_node->grammarSource = element;

    result.len = parse_terminal(sv);

    result.value = getValue(*result.root_node);

    result.ret = result.len == sv.size();

    return result;
  }

  MatchResult parse_rule(std::string_view sv, CstNode &parent,
                         IContext &c) const {

    assert(element && "The rule definition is missing !");
    auto i = parse_terminal(sv);
    if (i) {
      auto &node = parent.content.emplace_back();
      node.text = {sv.begin(), i.offset};
      node.grammarSource = this;
      // skip hidden nodes after the token
      i = c.skipHiddenNodes({i.offset, sv.end()}, parent);
    }

    return i;
  }

  using AbstractRule::operator=;
  void setValueConverter(std::function<T(std::string_view)> &&value_converter) {
    _value_converter = std::move(value_converter);
  }

private:
  std::function<T(std::string_view)> _value_converter =
      initializeDefaultValueConverter();

  std::function<T(std::string_view)> initializeDefaultValueConverter() {
    // initialize the value converter for standard types
    if constexpr (std::is_same_v<T, std::string_view>) {
      return [](std::string_view sv) { return sv; };
    } else if constexpr (std::is_same_v<T, std::string>) {
      return [](std::string_view sv) { return std::string(sv); };
    } else if constexpr (std::is_same_v<T, bool>) {
      return [](std::string_view sv) { return sv == "true"; };
    } else if constexpr (std::is_integral_v<T>) {
      return [](std::string_view sv) {
        T value;
        auto [ptr, ec] =
            std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc()) {
          throw std::invalid_argument("Conversion failed");
        }
        return value;
      };
    }
    return [this](std::string_view) -> T {
      throw std::logic_error("ValueConvert not provided for rule " + getName());
    };
  }
};
} // namespace pegium::parser