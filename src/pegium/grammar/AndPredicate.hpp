#pragma once
#include <pegium/grammar/IGrammarElement.hpp>
namespace pegium::grammar {

template <typename Element>
  requires IsGrammarElement<Element>
struct AndPredicate final : IGrammarElement {
  constexpr ~AndPredicate() override = default;
  explicit constexpr AndPredicate(Element &&element)
      : _element{forwardGrammarElement<Element>(element)} {}

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &,
                                   IContext &c) const override {
    CstNode node;
    return success(_element.parse_rule(sv, node, c)) ? 0 : PARSE_ERROR;
  }
  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    return success(_element.parse_terminal(sv)) ? 0 : PARSE_ERROR;
  }
  void print(std::ostream &os) const override { os << '&' << _element; }

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::AndPredicate;
  }

private:
  GrammarElementType<Element> _element;
};

template <typename Element>
  requires IsGrammarElement<Element>
constexpr auto operator~(Element &&element) {
  return AndPredicate<Element>{std::forward<Element>(element)};
}
} // namespace pegium::grammar