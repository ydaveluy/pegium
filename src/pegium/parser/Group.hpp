#pragma once
#include <pegium/grammar/Group.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <string_view>

namespace pegium::parser {

template <ParseExpression... Elements> struct Group final : grammar::Group {
  static constexpr bool nullable = (... && std::remove_cvref_t<Elements>::nullable);
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");

  constexpr explicit Group(std::tuple<Elements...> &&elements)
      : _elements{std::move(elements)} {}

  constexpr Group(Group &&) noexcept = default;
  constexpr Group(const Group &) = default;
  constexpr Group &operator=(Group &&) noexcept = default;
  constexpr Group &operator=(const Group &) = default;


  bool rule(ParseContext &ctx) const {
    detail::stepTraceInc(detail::StepCounter::GroupRecoverCalls);
    if (ctx.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::GroupStrictPasses);
      return rule_strict(ctx);
    }

    const auto entry = ctx.mark();
    PEGIUM_RECOVERY_TRACE("[group rule] enter offset=",
                          ctx.cursorOffset(), " allowI=",
                          ctx.allowInsert, " allowD=",
                          ctx.allowDelete);
    if (rule_editable_impl(ctx)) {
      detail::stepTraceInc(detail::StepCounter::GroupEditablePasses);
      PEGIUM_RECOVERY_TRACE("[group rule] editable success offset=",
                            ctx.cursorOffset());
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::GroupEditablePasses);

    PEGIUM_RECOVERY_TRACE("[group rule] fail offset=",
                          ctx.cursorOffset());
    ctx.rewind(entry);
    return false;
  }

  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {
    return parse_terminal_impl(begin, end, MatchResult::success(begin));
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
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
      auto next_r = std::get<I>(_elements).terminal(r.offset, end);
      return next_r.IsValid()
                 ? parse_terminal_impl<I + 1>(begin, end, next_r)
                 : next_r;
    }
  }

  template <std::size_t I = 0>
  bool rule_strict_impl(ParseContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      if (!std::get<I>(_elements).rule(ctx)) {
        return false;
      }
      return rule_strict_impl<I + 1>(ctx);
    }
  }

  bool rule_strict(ParseContext &ctx) const {
    const auto mark = ctx.mark();
    if (!rule_strict_impl(ctx)) {
      ctx.rewind(mark);
      return false;
    }
    return true;
  }

  bool rule_editable_impl(ParseContext &ctx) const {
    return rule_strict_impl(ctx);
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
