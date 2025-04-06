#pragma once
#include <pegium/grammar/IGrammarElement.hpp>
namespace pegium::grammar {

struct AbstractAndPredicate : IGrammarElement {
  constexpr AbstractAndPredicate(const IGrammarElement *element)
      : _element{element} {}
  const IGrammarElement *getElement() const noexcept;
  void print(std::ostream &os) const final;

  GrammarElementKind getKind() const noexcept final;

private:
  const IGrammarElement *_element;
};

template <typename Element>
  requires IsGrammarElement<Element>
struct AndPredicate final : AbstractAndPredicate {
  constexpr ~AndPredicate() override = default;
  explicit constexpr AndPredicate(Element &&element)
      : AbstractAndPredicate{&_element}, _element{element} {}

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &,
                                   IContext &c) const override {
    CstNode node;
    return _element.parse_rule(sv, node, c) ? MatchResult::success(sv.begin())
                                            : MatchResult::failure(sv.begin());
  }
  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    return _element.parse_terminal(sv) ? MatchResult::success(sv.begin())
                                       : MatchResult::failure(sv.begin());
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