#pragma once

#include <limits>
#include <pegium/grammar/Repetition.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/StepTrace.hpp>

namespace pegium::parser {

template <std::size_t min, std::size_t max, ParseExpression Element>
struct Repetition final : grammar::Repetition {
  static constexpr bool nullable =
      (min == 0) || std::remove_cvref_t<Element>::nullable;
  static_assert(!std::remove_cvref_t<Element>::nullable,
                "A Repetition cannot be initialized with a nullable element.");
  static_assert(!(min == 0 && max == 0),
                "A Repetition cannot have both min and max set to 0.");
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

  bool rule(ParseContext &ctx) const {
    detail::stepTraceInc(detail::StepCounter::RepetitionRecoverCalls);
    if (ctx.isStrictNoEditMode()) {
      return rule_strict_impl(ctx);
    }

    return rule_editable_impl(ctx);
  }

  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {
    // optional
    if constexpr (is_optional) {
      auto result = _element.terminal(begin, end);
      return result.IsValid() ? result : MatchResult::success(begin);
    }
    // zero or more
    else if constexpr (is_star) {
      auto result = MatchResult::success(begin);
      while (true) {
        auto r = _element.terminal(result.offset, end);
        if (!r.IsValid()) {
          break;
        }
        result = r;
      }
      return result;
    }
    // one or more
    else if constexpr (is_plus) {

      auto result = _element.terminal(begin, end);
      if (!result.IsValid())
        return result;

      while (true) {
        auto r = _element.terminal(result.offset, end);
        if (!r.IsValid()) {
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
        result = _element.terminal(result.offset, end);
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
        result = _element.terminal(result.offset, end);
        if (!result.IsValid()) {
          return result;
        }
      }

      for (; count < max; ++count) {
        auto r = _element.terminal(result.offset, end);
        if (!r.IsValid()) {
          break;
        }
        result = r;
      }

      return result;
    }
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
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

  bool rule_strict_impl(ParseContext &ctx) const {
    if constexpr (is_optional) {
      (void)_element.rule(ctx);
      return true;
    } else if constexpr (is_star) {
      while (_element.rule(ctx)) {
        // loop until the element cannot be recovered anymore
      }
      return true;
    } else if constexpr (is_plus) {
      if (!_element.rule(ctx)) {
        return false;
      }
      while (_element.rule(ctx)) {
        // loop until the element cannot be recovered anymore
      }
      return true;
    } else if constexpr (is_fixed) {
      for (std::size_t i = 0; i < min; ++i) {
        if (!_element.rule(ctx)) {
          return false;
        }
      }
      return true;
    } else {
      std::size_t count = 0;
      for (; count < min; ++count) {
        if (!_element.rule(ctx)) {
          return false;
        }
      }
      for (; count < max; ++count) {
        if (!_element.rule(ctx)) {
          break;
        }
      }
      return true;
    }
  }

  bool rule_editable_impl(ParseContext &ctx) const {
    if constexpr (is_optional) {
      (void)_element.rule(ctx);
      return true;
    } else if constexpr (is_star) {
      PEGIUM_RECOVERY_TRACE("[repeat * rule] enter offset=",
                            ctx.cursorOffset());
      while (true) {
        const char *const before = ctx.cursor();
        const auto mark = ctx.mark();
        const bool allowInsert = ctx.allowInsert;
        ctx.allowInsert = false;
        const bool elementRecovered = _element.rule(ctx);
        ctx.allowInsert = allowInsert;
        if (elementRecovered && ctx.cursor() != before) {
          PEGIUM_RECOVERY_TRACE("[repeat * rule] element matched offset=",
                                ctx.cursorOffset());
          continue;
        }
        ctx.rewind(mark);
        PEGIUM_RECOVERY_TRACE("[repeat * rule] stop offset=",
                              ctx.cursorOffset());
        break;
      }
      return true;
    } else if constexpr (is_plus) {
      const auto firstMark = ctx.mark();
      const char *const firstBefore = ctx.cursor();
      const bool allowInsert = ctx.allowInsert;
      ctx.allowInsert = false;
      if (!_element.rule(ctx) ||
          ctx.cursor() == firstBefore) {
        ctx.allowInsert = allowInsert;
        ctx.rewind(firstMark);
        PEGIUM_RECOVERY_TRACE("[repeat + rule] first element failed offset=",
                              ctx.cursorOffset());
        return false;
      }
      ctx.allowInsert = allowInsert;
      PEGIUM_RECOVERY_TRACE("[repeat + rule] first element ok offset=",
                            ctx.cursorOffset());
      while (true) {
        const char *const before = ctx.cursor();
        const auto mark = ctx.mark();
        const bool allowInsert = ctx.allowInsert;
        ctx.allowInsert = false;
        const bool elementRecovered = _element.rule(ctx);
        ctx.allowInsert = allowInsert;
        if (elementRecovered && ctx.cursor() != before) {
          PEGIUM_RECOVERY_TRACE("[repeat + rule] element matched offset=",
                                ctx.cursorOffset());
          continue;
        }
        ctx.rewind(mark);
        PEGIUM_RECOVERY_TRACE("[repeat + rule] stop offset=",
                              ctx.cursorOffset());
        break;
      }
      return true;
    } else if constexpr (is_fixed) {
      for (std::size_t i = 0; i < min; ++i) {
        if (!_element.rule(ctx)) {
          return false;
        }
      }
      return true;
    } else {
      std::size_t count = 0;
      for (; count < min; ++count) {
        if (!_element.rule(ctx)) {
          return false;
        }
      }
      for (; count < max; ++count) {
        const char *const before = ctx.cursor();
        if (!_element.rule(ctx) ||
            ctx.cursor() == before) {
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
