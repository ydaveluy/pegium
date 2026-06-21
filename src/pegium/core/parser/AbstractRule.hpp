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

  // Rule body's nullability must match the rule's declared kind.
  // The SFINAE constraint is what makes `requires { Rule{...} }` probes
  // (used by the compile-time guard tests) report `false` for a mismatched
  // body.
  // Hint to users hitting the resulting "no matching constructor" error:
  // use NullableRule<T> for a nullable body, Rule<T> for a non-nullable one.
  template <Expression Element>
    requires(std::remove_cvref_t<Element>::nullable == Nullable)
  AbstractRule(std::string_view name, Element &&element)
      : _name(name) {
    _wrapper.set(std::forward<Element>(element));
  }

  AbstractRule(const AbstractRule &) = default;
  AbstractRule(AbstractRule&&) noexcept = default;
  AbstractRule &operator=(const AbstractRule &) = default;
  AbstractRule &operator=(AbstractRule&&) noexcept = default;

  ~AbstractRule() override = default;

  template <Expression Element>
    requires(std::remove_cvref_t<Element>::nullable == Nullable)
  AbstractRule &operator=(Element &&element) {
    _wrapper.set(std::forward<Element>(element));
    return *this;
  }

  const grammar::AbstractElement *getElement() const noexcept override {
    return _wrapper.element();
  }

  // Forward the negative-lookahead "match at the cursor" probe through the
  // rule reference to its body. Without this a `!SomeRule` guard falls back to
  // the strict probe and cannot fuzzy-reject a truncated/typoed keyword the
  // way an inline `!"kw"_kw` / `!(a | b)` guard does. Only reached from the
  // recovery/tracked NotPredicate path, so the nominal parse path is unaffected.
  template <typename Context>
    requires(std::same_as<std::remove_cvref_t<Context>, TrackedParseContext> ||
             RecoveryParseModeContext<Context>)
  bool probeMatchHere(Context &ctx) const {
    return _wrapper.probe_match_here(ctx);
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
