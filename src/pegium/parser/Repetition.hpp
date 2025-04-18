#pragma once

#include <limits>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <std::size_t min, std::size_t max, ParserExpression Element>
struct Repetition final : grammar::Repetition {

  constexpr explicit Repetition(Element &&element)
      : _element{std::forward<Element>(element)} {}

  constexpr Repetition(Repetition &&) = default;
  constexpr Repetition(const Repetition &) = default;
  constexpr Repetition &operator=(Repetition &&) = default;
  constexpr Repetition &operator=(const Repetition &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {

    // optional
    if constexpr (min == 0 && max == 1) {
      auto result = _element.parse_rule(sv, parent, c);
      return result ? result : MatchResult::success(sv.begin());
    }
    // zero or more
    else if constexpr (min == 0 &&
                       max == std::numeric_limits<std::size_t>::max()) {

      MatchResult result = MatchResult::success(sv.begin());
      const auto end = sv.end();
      while (auto len = _element.parse_rule({result.offset, end}, parent, c)) {
        result = len;
      }
      return result;
    }
    // one or more
    else if constexpr (min == 1 &&
                       max == std::numeric_limits<std::size_t>::max()) {

      auto result = _element.parse_rule(sv, parent, c);
      if (!result)
        return result;
      const auto end = sv.end();
      while (auto len = _element.parse_rule({result.offset, end}, parent, c)) {
        result = len;
      }
      return result;
    }
    // other cases
    else {
      std::size_t count = 0;
      MatchResult result = MatchResult::success(sv.begin());
      const auto end = sv.end();

      while (count < min) {
        result = _element.parse_rule({result.offset, end}, parent, c);
        if (!result) {
          return result;
        }
        count++;
      }
      while (count < max) {
        auto len = _element.parse_rule({result.offset, end}, parent, c);
        if (!len) {

          break;
        }
        result = len;
        count++;
      }
      return result;
    }
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    // optional
    if constexpr (min == 0 && max == 1) {
      auto result = _element.parse_terminal(sv);
      return result ? result : MatchResult::success(sv.begin());
    }
    // zero or more
    else if constexpr (min == 0 &&
                       max == std::numeric_limits<std::size_t>::max()) {

      MatchResult result = MatchResult::success(sv.begin());

      const auto end = sv.end();
      while (auto len = _element.parse_terminal({result.offset, end})) {
        result = len;
      }
      return result;
    }
    // one or more
    else if constexpr (min == 1 &&
                       max == std::numeric_limits<std::size_t>::max()) {

      auto result = _element.parse_terminal(sv);
      if (!result)
        return result;

      const auto end = sv.end();
      while (auto len = _element.parse_terminal({result.offset, end})) {
        result = len;
      }
      return result;
    }
    // other cases
    else {

      MatchResult result = MatchResult::success(sv.begin());
      std::size_t count = 0;
      const auto end = sv.end();
      while (count < min) {
        result = _element.parse_terminal({result.offset, end});
        if (!result) {
          return result;
        }
        count++;
      }

      while (count < max) {
        auto len = _element.parse_terminal({result.offset, end});
        if (!len) {
          break;
        }
        result = len;
        count++;
      }

      return result;
    }
  }
  void print(std::ostream &os) const override {
    os << _element;

    if constexpr (min == 0 && max == 1)
      os << '?';
    else if constexpr (min == 0 &&
                       max == std::numeric_limits<std::size_t>::max())
      os << '*';
    else if constexpr (min == 1 &&
                       max == std::numeric_limits<std::size_t>::max())
      os << '+';
    else if constexpr (min == max)
      os << '{' << min << '}';
    else if constexpr (max == std::numeric_limits<std::size_t>::max())
      os << '{' << min << ",}";
    else
      os << '{' << min << ',' << max << '}';
  }

private:
  ParserExpressionHolder<Element> _element;
};

/// Make the `element` optional (repeated zero or one)
/// @tparam Element
/// @param element the element to be optional
/// @return The created Repetition
template <ParserExpression Element> constexpr auto option(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` zero or more
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element> constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <ParserExpression Element> constexpr auto some(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more using a `separator`:
/// `element (separator element)*`
/// @tparam Element
/// @tparam Sep
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return The created Repetition
template <ParserExpression Element, ParserExpression Sep>
constexpr auto some(Element &&element, Sep &&separator) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(separator) + std::forward<Element>(element));
}

/// Repeat the `element` zero or more using a `separator`
/// `(element (separator element)*)?`
/// @tparam Element
/// @tparam Sep
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return The created Repetition
template <ParserExpression Element, ParserExpression Sep>
constexpr auto many(Element &&element, Sep &&separator) {
  return option(
      some(std::forward<Element>(element), std::forward<Sep>(separator)));
}

/// Repeat the `element` `count` times.
/// @tparam count the count of repetions
/// @tparam Element
/// @param element the elements to be repeated
/// @return The created Repetition
template <std::size_t count, ParserExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<count, count, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` between `min` and `max` times.
/// @tparam min the min number of occurence (inclusive)
/// @tparam max the maw number of occurence (inclusive)
/// @tparam Element
/// @param element the elements to be repeated
/// @return The created Repetition
template <std::size_t min, std::size_t max, ParserExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser