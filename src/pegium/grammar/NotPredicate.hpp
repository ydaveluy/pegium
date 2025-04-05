#pragma once

#include <pegium/grammar/IGrammarElement.hpp>
namespace pegium::grammar {

template <typename Element>
  requires IsGrammarElement<Element>
struct NotPredicate final : IGrammarElement {
  constexpr ~NotPredicate() override = default;
  explicit constexpr NotPredicate(Element &&element)
      : _element{element} {}

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &,
                                   IContext &c) const override {
    CstNode node;
    return _element.parse_rule(sv, node, c) ? MatchResult::failure(sv.begin())
                                            : MatchResult::success(sv.begin());
  }

  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    return _element.parse_terminal(sv) ? MatchResult::failure(sv.begin())
                                       : MatchResult::success(sv.begin());
  }
  void print(std::ostream &os) const override { os << '!' << _element; }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::NotPredicate;
  }

private:
  GrammarElementType<Element> _element;
};

template <typename Element>
  requires IsGrammarElement<Element>
constexpr auto operator!(Element &&element) {
  return NotPredicate<Element>{std::forward<Element>(element)};
}

} // namespace pegium::grammar