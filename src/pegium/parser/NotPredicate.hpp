#pragma once

#include <pegium/grammar/NotPredicate.hpp>
#include <pegium/parser/AbstractElement.hpp>
namespace pegium::parser {
/*struct AbstractNotPredicate : IGrammarElement {

  const IGrammarElement *getElement() const noexcept { return _element; };
  void print(std::ostream &os) const override { os << '!' << *_element; }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::NotPredicate;
  }

protected:
  const IGrammarElement *_element;
};*/
template <ParserExpression Element>
struct NotPredicate final : grammar::NotPredicate {
  // constexpr ~NotPredicate() override = default;
  explicit constexpr NotPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr NotPredicate(NotPredicate &&) = default;
  constexpr NotPredicate(const NotPredicate &) = default;
  constexpr NotPredicate &operator=(NotPredicate &&) = default;
  constexpr NotPredicate &operator=(const NotPredicate &) = default;
  
  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &,
                                   IContext &c) const {
    CstNode node;
    return _element.parse_rule(sv, node, c) ? MatchResult::failure(sv.begin())
                                            : MatchResult::success(sv.begin());
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return _element.parse_terminal(sv) ? MatchResult::failure(sv.begin())
                                       : MatchResult::success(sv.begin());
  }

private:
  ParserExpressionHolder<Element> _element;
};

template <ParserExpression Element>
constexpr auto operator!(Element &&element) {
  return NotPredicate<Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser