#pragma once

#include <limits>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <std::size_t min, std::size_t max, ParserExpression Element>
struct Repetition final : grammar::Repetition {

  constexpr explicit Repetition(Element &&element)
      : _element{std::forward<Element>(element)} {}

  const AbstractElement *getElement() const noexcept override {
    return &_element;
  }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    std::size_t count = 0;
    MatchResult i = MatchResult::success(sv.begin());
    // auto size = parent.content.size();
    while (count < min) {
      i = _element.parse_rule({i.offset, sv.end()}, parent, c);
      if (!i) {
        // parent.content.resize(size);
        // assert(size == parent.content.size());
        return i;
      }
      count++;
    }
    if constexpr (max == std::numeric_limits<std::size_t>::max()) {
      while (auto len = _element.parse_rule({i.offset, sv.end()}, parent, c)) {
        i = len;
      }
    } else {
      while (count < max) {
        // size = parent.content.size();
        auto len = _element.parse_rule({i.offset, sv.end()}, parent, c);
        if (!len) {
          // parent.content.resize(size);
          // assert(size == parent.content.size());
          break;
        }
        i = len;
        count++;
      }
    }
    return i;
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    std::size_t count = 0;
    MatchResult i = MatchResult::success(sv.begin());
    while (count < min) {
      i = _element.parse_terminal({i.offset, sv.end()});
      if (!i) {
        return i;
      }
      count++;
    }

    if constexpr (max == std::numeric_limits<std::size_t>::max()) {
      while (auto len = _element.parse_terminal({i.offset, sv.end()})) {
        i = len;
      }
    } else {
      while (count < max) {
        auto len = _element.parse_terminal({i.offset, sv.end()});
        if (!len) {
          break;
        }
        i = len;
        count++;
      }
    }
    return i;
  }

private:
  ParserExpressionHolder<Element> _element;
};

/// Create an option (zero or one)
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element> constexpr auto opt(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Create a repetition of zero or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element> constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of one or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element>
constexpr auto at_least_one(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of one or more elements with a separator
/// `element (sep element)*`
/// @tparam Element
/// @tparam Sep
/// @param sep the separator to be used between elements
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element, ParserExpression Sep>
constexpr auto at_least_one_sep(Element &&element, Sep &&sep) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(sep) + std::forward<Element>(element));
}

/// Create a repetition of zero or more elements with a separator
/// `(element (sep element)*)?`
/// @tparam Element
/// @tparam Sep
/// @param sep the separator to be used between elements
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element, ParserExpression Sep>
constexpr auto many_sep(Element &&element, Sep &&sep) {
  return opt(
      at_least_one_sep(std::forward<Element>(element), std::forward<Sep>(sep)));
}

/// Create a custom repetition with count elements.
/// @tparam Element
/// @param element the elements to be repeated
/// @param count the count of repetions
/// @return The created Repetition
template <std::size_t count, ParserExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<count, count, Element>{std::forward<Element>(element)};
}

/// Create a custom repetition with min and max.
/// @tparam Element
/// @param element the elements to be repeated
/// @param min the min number of occurence (inclusive)
/// @param max the maw number of occurence (inclusive)
/// @return The created Repetition
template <std::size_t min, std::size_t max, ParserExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser