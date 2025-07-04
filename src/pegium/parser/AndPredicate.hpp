#pragma once
#include <memory>
#include <pegium/grammar/AndPredicate.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <ParseExpression Element>
struct AndPredicate final : grammar::AndPredicate {
  // constexpr ~AndPredicate() override = default;
  explicit constexpr AndPredicate(Element &&element)
      : _element{std::forward<Element>(element)} {}
  constexpr AndPredicate(AndPredicate &&) = default;
  constexpr AndPredicate(const AndPredicate &) = default;
  constexpr AndPredicate &operator=(AndPredicate &&) = default;
  constexpr AndPredicate &operator=(const AndPredicate &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
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
  ParseExpressionHolder<Element> _element;
};

template <ParseExpression Element> constexpr auto operator&(Element &&element) {
  return AndPredicate<Element>{std::forward<Element>(element)};
}
} // namespace pegium::parser