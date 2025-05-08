#pragma once
#include <memory>
#include <pegium/grammar/CrossReference.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <typename T, ParseExpression Element>
struct CrossReference final : grammar::CrossReference {
  // constexpr ~AndPredicate() override = default;
  explicit constexpr CrossReference(Element &&element)
      : _element{std::forward<Element>(element)} {}

      constexpr CrossReference(CrossReference &&) = default;
      constexpr CrossReference(const CrossReference &) = default;
      constexpr CrossReference &operator=(CrossReference &&) = default;
      constexpr CrossReference &operator=(const CrossReference &) = default;
      
  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }

  constexpr MatchResult parse_rule(std::string_view sv, const CstNode &parent,
                                   IContext &c) const {

    return _element.parse_rule(sv, parent, c);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return _element.parse_terminal(sv);
  }

private:
  ParseExpressionHolder<Element> _element;
};

template <typename T, ParseExpression Element>
constexpr auto xref(Element &&element) {
  return CrossReference<T, Element>{std::forward<Element>(element)};
}
} // namespace pegium::parser