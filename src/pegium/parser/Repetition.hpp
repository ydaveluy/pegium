#pragma once

#include <limits>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/StepTrace.hpp>

namespace pegium::parser {

template <std::size_t min, std::size_t max, ParseExpression Element>
struct Repetition final : grammar::Repetition {

  constexpr explicit Repetition(Element &&element)
      : _element{std::forward<Element>(element)} {}

  constexpr Repetition(Repetition &&) noexcept = default;
  constexpr Repetition(const Repetition &) = default;
  constexpr Repetition &operator=(Repetition &&) noexcept = default;
  constexpr Repetition &operator=(const Repetition &) = default;

  const AbstractElement *getElement() const noexcept override {
    return std::addressof(_element);
  }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }

  constexpr bool parse_rule(ParseState &s) const {
    // optional
    if constexpr (is_optional) {
      (void)_element.parse_rule(s);
      return true;
    }
    // zero or more
    else if constexpr (is_star) {
      while (true) {
        const char *const before = s.cursor();
        if (!_element.parse_rule(s) || s.cursor() == before) {
          break;
        }
      }
      return true;
    }
    // one or more
    else if constexpr (is_plus) {

      if (!_element.parse_rule(s)) {
        return false;
      }
      while (true) {
        const char *const before = s.cursor();
        if (!_element.parse_rule(s) || s.cursor() == before) {
          break;
        }
      }
      return true;
    }
    // only min/max times
    else if constexpr (is_fixed) {
      for (std::size_t i = 0; i < min; ++i) {
        if (!_element.parse_rule(s)) {
          return false;
        }
      }
      return true;
    }
    // other cases
    else {
      std::size_t count = 0;

      for (; count < min; ++count) {
        if (!_element.parse_rule(s)) {
          return false;
        }
      }
      for (; count < max; ++count) {
        const char *const before = s.cursor();
        if (!_element.parse_rule(s) || s.cursor() == before) {
          break;
        }
      }
      return true;
    }
  }

  bool recover(RecoverState &recoverState) const {
    detail::stepTraceInc(detail::StepCounter::RepetitionRecoverCalls);
    if (recoverState.isStrictNoEditMode()) {
      return recover_strict_impl(recoverState);
    }

    return recover_editable_impl(recoverState);
  }

  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    // optional
    if constexpr (is_optional) {
      auto result = _element.parse_terminal(begin, end);
      return result.IsValid() ? result : MatchResult::success(begin);
    }
    // zero or more
    else if constexpr (is_star) {
      auto result = MatchResult::success(begin);
      while (true) {
        auto r = _element.parse_terminal(result.offset, end);
        if (!r.IsValid() || r.offset == result.offset) {
          break;
        }
        result = r;
      }
      return result;
    }
    // one or more
    else if constexpr (is_plus) {

      auto result = _element.parse_terminal(begin, end);
      if (!result.IsValid())
        return result;

      while (true) {
        auto r = _element.parse_terminal(result.offset, end);
        if (!r.IsValid() || r.offset == result.offset) {
          break;
        }
        result = r;
      }
      return result;
    }
    // only min/max times
    else if constexpr (is_fixed) {
      auto result = MatchResult::success(begin);
      for (std::size_t i = 0; i < min; ++i) {
        result = _element.parse_terminal(result.offset, end);
        if (!result.IsValid()) {
          return result;
        }
      }
      return result;
    }
    // other cases
    else {

      MatchResult result = MatchResult::success(begin);
      std::size_t count = 0;
      for (; count < min; ++count) {
        result = _element.parse_terminal(result.offset, end);
        if (!result.IsValid()) {
          return result;
        }
      }

      for (; count < max; ++count) {
        auto r = _element.parse_terminal(result.offset, end);
        if (!r.IsValid()) {
          break;
        }
        result = r;
      }

      return result;
    }
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }
  void print(std::ostream &os) const override {
    os << _element;

    if constexpr (is_optional)
      os << '?';
    else if constexpr (is_star)
      os << '*';
    else if constexpr (is_plus)
      os << '+';
    else if constexpr (is_fixed)
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
  static constexpr bool is_fixed = (min == max && min > 0);

  bool recover_strict_impl(RecoverState &recoverState) const {
    if constexpr (is_optional) {
      (void)_element.recover(recoverState);
      return true;
    } else if constexpr (is_star) {
      while (true) {
        const char *const before = recoverState.cursor();
        if (!_element.recover(recoverState) || recoverState.cursor() == before) {
          break;
        }
      }
      return true;
    } else if constexpr (is_plus) {
      if (!_element.recover(recoverState)) {
        return false;
      }
      while (true) {
        const char *const before = recoverState.cursor();
        if (!_element.recover(recoverState) || recoverState.cursor() == before) {
          break;
        }
      }
      return true;
    } else if constexpr (is_fixed) {
      for (std::size_t i = 0; i < min; ++i) {
        if (!_element.recover(recoverState)) {
          return false;
        }
      }
      return true;
    } else {
      std::size_t count = 0;
      for (; count < min; ++count) {
        if (!_element.recover(recoverState)) {
          return false;
        }
      }
      for (; count < max; ++count) {
        const char *const before = recoverState.cursor();
        if (!_element.recover(recoverState) || recoverState.cursor() == before) {
          break;
        }
      }
      return true;
    }
  }

  bool recover_editable_impl(RecoverState &recoverState) const {
    if constexpr (is_optional) {
      (void)_element.recover(recoverState);
      return true;
    } else if constexpr (is_star) {
      PEGIUM_RECOVERY_TRACE("[repeat * recover] enter offset=",
                            recoverState.cursorOffset());
      while (true) {
        const char *const before = recoverState.cursor();
        const auto mark = recoverState.mark();
        const bool allowInsert = recoverState.allowInsert;
        recoverState.allowInsert = false;
        const bool elementRecovered = _element.recover(recoverState);
        recoverState.allowInsert = allowInsert;
        if (elementRecovered && recoverState.cursor() != before) {
          PEGIUM_RECOVERY_TRACE("[repeat * recover] element matched offset=",
                                recoverState.cursorOffset());
          continue;
        }
        recoverState.rewind(mark);
        PEGIUM_RECOVERY_TRACE("[repeat * recover] stop offset=",
                              recoverState.cursorOffset());
        break;
      }
      return true;
    } else if constexpr (is_plus) {
      const auto firstMark = recoverState.mark();
      const char *const firstBefore = recoverState.cursor();
      const bool allowInsert = recoverState.allowInsert;
      recoverState.allowInsert = false;
      if (!_element.recover(recoverState) ||
          recoverState.cursor() == firstBefore) {
        recoverState.allowInsert = allowInsert;
        recoverState.rewind(firstMark);
        PEGIUM_RECOVERY_TRACE("[repeat + recover] first element failed offset=",
                              recoverState.cursorOffset());
        return false;
      }
      recoverState.allowInsert = allowInsert;
      PEGIUM_RECOVERY_TRACE("[repeat + recover] first element ok offset=",
                            recoverState.cursorOffset());
      while (true) {
        const char *const before = recoverState.cursor();
        const auto mark = recoverState.mark();
        const bool allowInsert = recoverState.allowInsert;
        recoverState.allowInsert = false;
        const bool elementRecovered = _element.recover(recoverState);
        recoverState.allowInsert = allowInsert;
        if (elementRecovered && recoverState.cursor() != before) {
          PEGIUM_RECOVERY_TRACE("[repeat + recover] element matched offset=",
                                recoverState.cursorOffset());
          continue;
        }
        recoverState.rewind(mark);
        PEGIUM_RECOVERY_TRACE("[repeat + recover] stop offset=",
                              recoverState.cursorOffset());
        break;
      }
      return true;
    } else if constexpr (is_fixed) {
      for (std::size_t i = 0; i < min; ++i) {
        if (!_element.recover(recoverState)) {
          return false;
        }
      }
      return true;
    } else {
      std::size_t count = 0;
      for (; count < min; ++count) {
        if (!_element.recover(recoverState)) {
          return false;
        }
      }
      for (; count < max; ++count) {
        const char *const before = recoverState.cursor();
        if (!_element.recover(recoverState) || recoverState.cursor() == before) {
          break;
        }
      }
      return true;
    }
  }
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
