#pragma once

#include <limits>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <std::size_t min, std::size_t max, ParseExpression Element>
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
    if constexpr (is_optional) {
      auto result = _element.parse_rule(sv, parent, c);
      return result ? result : MatchResult::success(sv.begin());
    }
    // zero or more
    else if constexpr (is_star) {
      auto result = MatchResult::success(sv.begin());
      const auto end = sv.end();
      while (auto r = _element.parse_rule({result.offset, end}, parent, c)) {
        result = r;
      }
      return result;
    }
    // one or more
    else if constexpr (is_plus) {

      auto result = _element.parse_rule(sv, parent, c);
      if (!result)
        return result;
      const auto end = sv.end();
      while (auto r = _element.parse_rule({result.offset, end}, parent, c)) {
        result = r;
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
        ++count;
      }
      while (count < max) {
        auto r = _element.parse_rule({result.offset, end}, parent, c);
        if (!r) {
          break;
        }
        result = r;
        ++count;
      }
      return result;
    }
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    // optional
    if constexpr (is_optional) {
      auto result = _element.parse_terminal(sv);
      return result ? result : MatchResult::success(sv.begin());
    }
    // zero or more
    else if constexpr (is_star) {
      auto result = MatchResult::success(sv.begin());
      const auto end = sv.end();
      while (auto r = _element.parse_terminal({result.offset, end})) {
        result = r;
      }
      return result;
    }
    // one or more
    else if constexpr (is_plus) {

      auto result = _element.parse_terminal(sv);
      if (!result)
        return result;

      const auto end = sv.end();
      while (auto r = _element.parse_terminal({result.offset, end})) {
        result = r;
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
        ++count;
      }

      while (count < max) {
        auto r = _element.parse_terminal({result.offset, end});
        if (!r) {
          break;
        }
        result = r;
        ++count;
      }

      return result;
    }
  }
  void print(std::ostream &os) const override {
    os << _element;

    if constexpr (is_optional)
      os << '?';
    else if constexpr (is_star)
      os << '*';
    else if constexpr (is_plus)
      os << '+';
    else if constexpr (min == max)
      os << '{' << min << '}';
    else if constexpr (max == std::numeric_limits<std::size_t>::max())
      os << '{' << min << ",}";
    else
      os << '{' << min << ',' << max << '}';
  }

private:
  ParseExpressionHolder<Element> _element;

  static constexpr bool is_optional = (min == 0 && max == 1);
  static constexpr bool is_star =
      (min == 0 && max == std::numeric_limits<std::size_t>::max());
  static constexpr bool is_plus =
      (min == 1 && max == std::numeric_limits<std::size_t>::max());
};

/// Make the `element` optional (repeated zero or one)
/// @tparam Element the expression to repeat
/// @param element the element to be optional
/// @return a repetition of zero or one `element`.
template <ParseExpression Element> constexpr auto option(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` zero or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of zero or more `element`.
template <ParseExpression Element> constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more
/// @tparam Element the expression to repeat
/// @param element the element to be repeated
/// @return a repetition of one or more `element`.
template <ParseExpression Element> constexpr auto some(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Repeat the `element` one or more using a `separator`:
/// `element (separator element)*`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of one or more `element` with a `separator`.
template <ParseExpression Element, ParseExpression Sep>
constexpr auto some(Element &&element, Sep &&separator) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(separator) + std::forward<Element>(element));
}

/// Repeat the `element` zero or more using a `separator`
/// `(element (separator element)*)?`
/// @tparam Element the expression to repeat
/// @tparam Sep the expression to use as separator
/// @param element the element to be repeated
/// @param separator the separator to be used between elements
/// @return a repetition of zero or more `element` with a `separator`.
template <ParseExpression Element, ParseExpression Sep>
constexpr auto many(Element &&element, Sep &&separator) {
  return option(
      some(std::forward<Element>(element), std::forward<Sep>(separator)));
}

/// Repeat the `element` `count` times.
/// @tparam count the count of repetitions
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `count` `element`.
template <std::size_t count, ParseExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<count, count, Element>{std::forward<Element>(element)};
}

/// Repeat the `element` between `min` and `max` times.
/// @tparam min the min number of occurence (inclusive)
/// @tparam max the max number of occurence (inclusive)
/// @tparam Element the expression to repeat
/// @param element the elements to be repeated
/// @return a repetition of `min` to `max` `element`.
template <std::size_t min, std::size_t max, ParseExpression Element>
constexpr auto rep(Element &&element) {
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::parser