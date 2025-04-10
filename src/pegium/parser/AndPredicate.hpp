#pragma once
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/parser/AbstractElement.hpp>
namespace pegium::parser {

/*struct AbstractAndPredicate : IGrammarElement {
  constexpr explicit AbstractAndPredicate(const IGrammarElement *element)
      : _element{element} {}
  const IGrammarElement *getElement() const noexcept;
  void print(std::ostream &os) const final;

  GrammarElementKind getKind() const noexcept final;

private:
  const IGrammarElement *_element;
};*/

template <ParserExpression Element>
struct AndPredicate final : grammar::AndPredicate {
  // constexpr ~AndPredicate() override = default;
  explicit constexpr AndPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}

  const AbstractElement *getElement() const noexcept override {
    return &_element;
  }

  constexpr MatchResult parse_rule(std::string_view sv, const CstNode &,
                                   IContext &c) const {
    CstNode node;
    return _element.parse_rule(sv, node, c) ? MatchResult::success(sv.begin())
                                            : MatchResult::failure(sv.begin());
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return _element.parse_terminal(sv) ? MatchResult::success(sv.begin())
                                       : MatchResult::failure(sv.begin());
  }

private:
  ParserExpressionHolder<Element> _element;
};

template <ParserExpression Element>
constexpr auto operator~(Element &&element) {
  return AndPredicate<Element>{std::forward<Element>(element)};
}
} // namespace pegium::parser