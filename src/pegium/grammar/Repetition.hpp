#pragma once

#include <limits>
#include <pegium/grammar/IGrammarElement.hpp>

namespace pegium::grammar {

struct AbstractRepetition : IGrammarElement {
  constexpr AbstractRepetition(const IGrammarElement *element, std::size_t min,
                               std::size_t max)
      : _element{element}, _min{min}, _max{max} {}
  const IGrammarElement *getElement() const noexcept;
  std::size_t getMin() const noexcept;
  std::size_t getMax() const noexcept;
  void print(std::ostream &os) const final;

  GrammarElementKind getKind() const noexcept final;

private:
  const IGrammarElement *_element;
  std::size_t _min;
  std::size_t _max;
};

template <std::size_t min, std::size_t max, typename Element>
  requires IsGrammarElement<Element>
struct Repetition final : AbstractRepetition {
  constexpr ~Repetition() override = default;
  constexpr explicit Repetition(Element &&element)
      : AbstractRepetition{&_element, min, max}, _element{element} {}

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
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
    return i;
  }

  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    std::size_t count = 0;
    MatchResult i = MatchResult::success(sv.begin());
    while (count < min) {
      i = _element.parse_terminal({i.offset, sv.end()});
      if (!i) {
        return i;
      }
      count++;
    }
    while (count < max) {
      auto len = _element.parse_terminal({i.offset, sv.end()});
      if (!len) {
        break;
      }
      i = len;
      count++;
    }
    return i;
  }

private:
  GrammarElementType<Element> _element;
};

/// Create an option (zero or one)
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto opt(Element &&element) {
  return Repetition<0, 1, Element>{std::forward<Element>(element)};
}

/// Create a repetition of zero or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto many(Element &&element) {
  return Repetition<0, std::numeric_limits<std::size_t>::max(), Element>{
      std::forward<Element>(element)};
}

/// Create a repetition of one or more elements
/// @tparam Element
/// @param element the element to be repeated
/// @return The created Repetition
template <typename Element>
  requires(IsGrammarElement<Element>)
constexpr auto at_least_one(Element &&element) {
  return Repetition<1, std::numeric_limits<std::size_t>::max(), Element>{
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
  return Repetition<count, count, Element>{std::forward<Element>(element)};
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
  return Repetition<min, max, Element>{std::forward<Element>(element)};
}

} // namespace pegium::grammar