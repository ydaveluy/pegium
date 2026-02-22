#pragma once
#include <pegium/grammar/Group.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <string_view>

namespace pegium::parser {

template <ParseExpression... Elements> struct Group final : grammar::Group {
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");

  constexpr explicit Group(std::tuple<Elements...> &&elements)
      : _elements{std::move(elements)} {}

  constexpr Group(Group &&) noexcept = default;
  constexpr Group(const Group &) = default;
  constexpr Group &operator=(Group &&) noexcept = default;
  constexpr Group &operator=(const Group &) = default;

  constexpr bool parse_rule(ParseState &s) const {
    const auto mark = s.mark();
    if (!parse_rule_impl(s)) {
      s.rewind(mark);
      return false;
    }
    return true;
  }

  bool recover(RecoverState &recoverState) const {
    detail::stepTraceInc(detail::StepCounter::GroupRecoverCalls);
    if (recoverState.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::GroupStrictPasses);
      return recover_strict(recoverState);
    }

    const auto entry = recoverState.mark();
    PEGIUM_RECOVERY_TRACE("[group recover] enter offset=",
                          recoverState.cursorOffset(), " allowI=",
                          recoverState.allowInsert, " allowD=",
                          recoverState.allowDelete);
    if (recover_editable_impl(recoverState)) {
      detail::stepTraceInc(detail::StepCounter::GroupEditablePasses);
      PEGIUM_RECOVERY_TRACE("[group recover] editable success offset=",
                            recoverState.cursorOffset());
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::GroupEditablePasses);

    PEGIUM_RECOVERY_TRACE("[group recover] fail offset=",
                          recoverState.cursorOffset());
    recoverState.rewind(entry);
    return false;
  }

  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    return parse_terminal_impl(begin, end, MatchResult::success(begin));
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }
  void print(std::ostream &os) const override {
    os << '(';
    bool first = true;
    std::apply(
        [&](auto const &...elem) {
          ((os << (first ? "" : " "), os << elem, first = false), ...);
        },
        _elements);

    os << ')';
  }

private:
  std::tuple<Elements...> _elements;
  template <std::size_t I = 0>
  constexpr MatchResult parse_terminal_impl(const char *begin, const char *end,
                                            MatchResult r) const noexcept {
    if constexpr (I == sizeof...(Elements)) {
      return r;
    } else {
      auto next_r = std::get<I>(_elements).parse_terminal(r.offset, end);
      return next_r.IsValid()
                 ? parse_terminal_impl<I + 1>(begin, end, next_r)
                 : next_r;
    }
  }
  template <std::size_t I = 0>
  constexpr bool parse_rule_impl(ParseState &s) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      if (!std::get<I>(_elements).parse_rule(s)) {
        return false;
      }
      return parse_rule_impl<I + 1>(s);
    }
  }

  template <std::size_t I = 0>
  bool recover_strict_impl(RecoverState &recoverState) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      if (!std::get<I>(_elements).recover(recoverState)) {
        return false;
      }
      return recover_strict_impl<I + 1>(recoverState);
    }
  }

  bool recover_strict(RecoverState &recoverState) const {
    const auto mark = recoverState.mark();
    if (!recover_strict_impl(recoverState)) {
      recoverState.rewind(mark);
      return false;
    }
    return true;
  }

  bool recover_editable_impl(RecoverState &recoverState) const {
    return recover_strict_impl(recoverState);
  }

  template <ParseExpression... Rhs>
  friend constexpr auto operator+(Group &&lhs, Group<Rhs...> &&rhs) {
    return Group<Elements..., ParseExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParseExpression Rhs>
  friend constexpr auto operator+(Group &&lhs, Rhs &&rhs) {
    return Group<Elements..., ParseExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(std::forward<Rhs>(rhs)))};
  }
  template <ParseExpression Lhs>
  friend constexpr auto operator+(Lhs &&lhs, Group &&rhs) {
    return Group<ParseExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(std::forward<Lhs>(lhs)), std::move(rhs._elements))};
  }
};

template <ParseExpression Lhs, ParseExpression Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return Group<ParseExpressionHolder<Lhs>, ParseExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}
} // namespace pegium::parser
