#pragma once

#include <pegium/grammar/OrderedChoice.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <ParseExpression... Elements>
struct OrderedChoice final : grammar::OrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  
  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elements)
      : _elements{std::move(elements)} {}
  constexpr OrderedChoice(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice(const OrderedChoice &) = default;
  constexpr OrderedChoice &operator=(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice &operator=(const OrderedChoice &) = default;

  constexpr bool parse_rule(ParseState &s) const {
    return parse_rule_impl(s);
  }

  bool recover(RecoverState &recoverState) const {
    detail::stepTraceInc(detail::StepCounter::ChoiceRecoverCalls);
    if (recoverState.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
      return recover_strict(recoverState);
    }

    const auto entry = recoverState.mark();
    PEGIUM_RECOVERY_TRACE("[choice recover] enter offset=",
                          recoverState.cursorOffset(), " allowI=",
                          recoverState.allowInsert, " allowD=",
                          recoverState.allowDelete);

    const bool allowInsert = recoverState.allowInsert;
    const bool allowDelete = recoverState.allowDelete;
    recoverState.allowInsert = false;
    recoverState.allowDelete = false;
    detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
    if (recover_strict(recoverState)) {
      recoverState.allowInsert = allowInsert;
      recoverState.allowDelete = allowDelete;
      PEGIUM_RECOVERY_TRACE("[choice recover] strict success offset=",
                            recoverState.cursorOffset());
      return true;
    }
    recoverState.rewind(entry);
    recoverState.allowInsert = allowInsert;
    recoverState.allowDelete = allowDelete;

    if (recover_editable(recoverState)) {
      detail::stepTraceInc(detail::StepCounter::ChoiceEditablePasses);
      PEGIUM_RECOVERY_TRACE("[choice recover] editable success offset=",
                            recoverState.cursorOffset());
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::ChoiceEditablePasses);

    PEGIUM_RECOVERY_TRACE("[choice recover] fail offset=",
                          recoverState.cursorOffset());
    recoverState.rewind(entry);
    return false;
  }

  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    return parse_terminal_impl(begin, end);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
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
      auto r = std::get<I>(_elements).parse_terminal(begin, end);
      return r.IsValid() ? r : parse_terminal_impl<I + 1>(begin, end);
    }
  }

  template <std::size_t I = 0>
  constexpr bool parse_rule_impl(ParseState &s) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto mark = s.mark();
      if (std::get<I>(_elements).parse_rule(s)) {
        return true;
      }
      s.rewind(mark);
      return parse_rule_impl<I + 1>(s);
    }
  }

  template <std::size_t I = 0>
  bool recover_rule_impl(RecoverState &recoverState) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      const auto mark = recoverState.mark();
      if (std::get<I>(_elements).recover(recoverState)) {
        return true;
      }
      recoverState.rewind(mark);
      return recover_rule_impl<I + 1>(recoverState);
    }
  }

  bool recover_strict(RecoverState &recoverState) const {
    return recover_rule_impl(recoverState);
  }

  bool recover_editable(RecoverState &recoverState) const {
    return recover_rule_impl(recoverState);
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
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(std::forward<Rhs>(rhs)))};
  }
  template <ParseExpression Lhs>
  friend constexpr auto operator|(Lhs &&lhs, OrderedChoice &&rhs) {
    return OrderedChoice<ParseExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(std::forward<Lhs>(lhs)), std::move(rhs._elements))};
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
struct IsOrderedChoice
    : detail::IsOrderedChoiceRaw<std::remove_cvref_t<T>> {};

} // namespace pegium::parser
