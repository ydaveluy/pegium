#pragma once

#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <ParseExpression... Elements>
struct OrderedChoice final : grammar::OrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  static constexpr bool nullable = (... || std::remove_cvref_t<Elements>::nullable);

  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elements)
      : _elements{std::move(elements)} {}
  constexpr OrderedChoice(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice(const OrderedChoice &) = default;
  constexpr OrderedChoice &operator=(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice &operator=(const OrderedChoice &) = default;

  bool rule(ParseContext &ctx) const {
    detail::stepTraceInc(detail::StepCounter::ChoiceRecoverCalls);
    if (ctx.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
      return rule_strict(ctx);
    }

    const auto entry = ctx.mark();
    PEGIUM_RECOVERY_TRACE(
        "[choice rule] enter offset=", ctx.cursorOffset(),
        " allowI=", ctx.allowInsert,
        " allowD=", ctx.allowDelete);

    const bool allowInsert = ctx.allowInsert;
    const bool allowDelete = ctx.allowDelete;
    ctx.allowInsert = false;
    ctx.allowDelete = false;
    detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
    if (rule_strict(ctx)) {
      ctx.allowInsert = allowInsert;
      ctx.allowDelete = allowDelete;
      PEGIUM_RECOVERY_TRACE("[choice rule] strict success offset=",
                            ctx.cursorOffset());
      return true;
    }
    ctx.rewind(entry);
    ctx.allowInsert = allowInsert;
    ctx.allowDelete = allowDelete;

    if (rule_editable(ctx)) {
      detail::stepTraceInc(detail::StepCounter::ChoiceEditablePasses);
      PEGIUM_RECOVERY_TRACE("[choice rule] editable success offset=",
                            ctx.cursorOffset());
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::ChoiceEditablePasses);

    PEGIUM_RECOVERY_TRACE("[choice rule] fail offset=",
                          ctx.cursorOffset());
    ctx.rewind(entry);
    return false;
  }

  constexpr MatchResult terminal(const char *begin,
                                       const char *end) const noexcept {
    return parse_terminal_impl(begin, end);
  }
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
  }

  void print(std::ostream &os) const override {
    os << '(';
    bool first = true;
    std::apply(
        [&](auto const &...elems) {
          ((os << (first ? "" : " | "), os << elems, first = false), ...);
        },
        _elements);

    os << ')';
  }

private:
  std::tuple<Elements...> _elements;

  template <std::size_t I = 0>
  constexpr MatchResult parse_terminal_impl(const char *begin,
                                            const char *end) const noexcept {
    if constexpr (I == sizeof...(Elements)) {
      return MatchResult::failure(begin);
    } else {
      auto r = std::get<I>(_elements).terminal(begin, end);
      return r.IsValid() ? r : parse_terminal_impl<I + 1>(begin, end);
    }
  }

  template <std::size_t I = 0>
  bool rule_impl(ParseContext &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto mark = ctx.mark();
      if (std::get<I>(_elements).rule(ctx)) {
        return true;
      }
      ctx.rewind(mark);
      return rule_impl<I + 1>(ctx);
    }
  }

  bool rule_strict(ParseContext &ctx) const {
    return rule_impl(ctx);
  }

  bool rule_editable(ParseContext &ctx) const {
    return rule_impl(ctx);
  }

  template <ParseExpression... Rhs>
  friend constexpr auto operator|(OrderedChoice &&lhs,
                                  OrderedChoice<Rhs...> &&rhs) {
    return OrderedChoice<Elements..., ParseExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParseExpression Rhs>
  friend constexpr auto operator|(OrderedChoice &&lhs, Rhs &&rhs) {
    return OrderedChoice<Elements..., ParseExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements),
                       std::forward_as_tuple(std::forward<Rhs>(rhs)))};
  }
  template <ParseExpression Lhs>
  friend constexpr auto operator|(Lhs &&lhs, OrderedChoice &&rhs) {
    return OrderedChoice<ParseExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(std::forward<Lhs>(lhs)),
                       std::move(rhs._elements))};
  }
};

template <ParseExpression Lhs, ParseExpression Rhs>
constexpr auto operator|(Lhs &&lhs, Rhs &&rhs) {
  return OrderedChoice<ParseExpressionHolder<Lhs>, ParseExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

namespace detail {

template <typename T> struct IsOrderedChoiceRaw : std::false_type {};

template <typename... E>
struct IsOrderedChoiceRaw<OrderedChoice<E...>> : std::true_type {};

} // namespace detail

template <typename T>
struct IsOrderedChoice : detail::IsOrderedChoiceRaw<std::remove_cvref_t<T>> {};

} // namespace pegium::parser
