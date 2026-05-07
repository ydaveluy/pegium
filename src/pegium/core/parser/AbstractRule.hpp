#pragma once
/// Generic parser rule wrapper around one grammar rule interface.
#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace pegium::parser {

template <typename GrammarRule, bool Nullable = false>
  requires std::derived_from<GrammarRule, grammar::AbstractRule>
struct AbstractRule : GrammarRule {
  static constexpr bool nullable = Nullable;

  template <Expression Element>
  AbstractRule(std::string_view name, Element &&element)
      : _name(name) {
    static_assert(
        std::remove_cvref_t<Element>::nullable == Nullable,
        "Rule body's nullability must match the rule's declared kind. "
        "Use NullableRule<T> for a nullable body, Rule<T> for a "
        "non-nullable body.");
    _wrapper.set(std::forward<Element>(element));
  }

  AbstractRule(const AbstractRule &) = default;
  AbstractRule(AbstractRule&&) noexcept = default;
  AbstractRule &operator=(const AbstractRule &) = default;
  AbstractRule &operator=(AbstractRule&&) noexcept = default;

  ~AbstractRule() override = default;

  template <Expression Element>
  AbstractRule &operator=(Element &&element) {
    static_assert(
        std::remove_cvref_t<Element>::nullable == Nullable,
        "Rule body's nullability must match the rule's declared kind.");
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
