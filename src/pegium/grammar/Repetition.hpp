#pragma once

#include <limits>
#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

template <std::size_t min, std::size_t max, typename Element>
  requires IsGrammarElement<Element>
struct Repetition : IGrammarElement {
  constexpr ~Repetition() override = default;
  GrammarElementType<Element> element;
  constexpr explicit Repetition(Element &&element)
      : element{forwardGrammarElement<Element>(element)} {}

  constexpr std::size_t parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
    std::size_t count = 0;
    std::size_t i = 0;
    //auto size = parent.content.size();
    while (count < min) {
      auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
      if (fail(len)) {
        // parent.content.resize(size);
        //assert(size == parent.content.size());
        return len;
      }
      i += len;
      count++;
    }
    while (count < max) {
      //size = parent.content.size();
      auto len = element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
      if (fail(len)) {
        // parent.content.resize(size);
        //assert(size == parent.content.size());
        break;
      }
      i += len;
      count++;
    }
    return i;
  }

  constexpr std::size_t
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t count = 0;
    std::size_t i = 0;
    while (count < min) {
      auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
      if (fail(len)) {
        return len;
      }
      i += len;
      count++;
    }
    while (count < max) {
      auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
      if (fail(len)) {
        break;
      }
      i += len;
      count++;
    }
    return i;
  }
  void print(std::ostream &os) const override {
    os << element;
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

  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::Repetition;
  }
};

/// Create an option (zero or one)
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto opt(Element &&element) {
  return Repetition<0, 1, Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of zero or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(),
                    Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of one or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto at_least_one(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(),
                    Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of one or more elements with a separator
/// `element (sep element)*`
/// @tparam Element
/// @param sep the separator to be used between elements
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element, typename Sep>
  requires IsGrammarElement<Sep> && (IsGrammarElement<Element>)
constexpr auto at_least_one_sep(Element &&element, Sep &&sep) {
  return std::forward<Element>(element) +
         many(std::forward<Sep>(sep) + std::forward<Element>(element));
}

/// Create a repetition of zero or more elements with a separator
/// `(element (sep element)*)?`
/// @tparam Element
/// @param sep the separator to be used between elements
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element, typename Sep>
  requires IsGrammarElement<Sep> && (IsGrammarElement<Element>)
constexpr auto many_sep(Element &&element, Sep &&sep) {
  return opt(
      at_least_one_sep(std::forward<Element>(element), std::forward<Sep>(sep)));
}

/// Create a custom repetition with count elements.
/// @tparam Element
/// @param element the elements to be repeated
/// @param count the count of repetions
/// @return The created Repetition
template <std::size_t count, typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto rep(Element &&element) {
  return Repetition<count, count, Element>{
      std::forward<Element>(element)};
}

/// Create a custom repetition with min and max.
/// @tparam Element
/// @param element the elements to be repeated
/// @param min the min number of occurence (inclusive)
/// @param max the maw number of occurence (inclusive)
/// @return The created Repetition
template <std::size_t min, std::size_t max, typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto rep(Element &&element) {
  return Repetition<min, max, Element>{
      std::forward<Element>(element)};
}

} // namespace pegium::grammar