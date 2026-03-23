#pragma once
/// Generic parser rule wrapper around one grammar rule interface.
#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace pegium::parser {

template <typename GrammarRule>
  requires std::derived_from<GrammarRule, grammar::AbstractRule>
struct AbstractRule : GrammarRule {
  static constexpr bool nullable = false;

  template <NonNullableExpression Element>
  AbstractRule(std::string_view name, Element &&element)
      : _name(name) {
    _wrapper.set(std::forward<Element>(element));
  }

  AbstractRule(const AbstractRule &) = default;
  AbstractRule(AbstractRule&&) noexcept = default;
  AbstractRule &operator=(const AbstractRule &) = default;
  AbstractRule &operator=(AbstractRule&&) noexcept = default;

  ~AbstractRule() override = default;

  template <NonNullableExpression Element>
  AbstractRule &operator=(Element &&element) {
    _wrapper.set(std::forward<Element>(element));
    return *this;
  }

  const grammar::AbstractElement *getElement() const noexcept override {
    return _wrapper.element();
  }

  constexpr bool isNullable() const noexcept override {
    return nullable;
  }

  std::string_view getName() const noexcept override {
    return _name;
  }

protected:
  Wrapper _wrapper;

private:
  std::string _name;
};
} // namespace pegium::parser
