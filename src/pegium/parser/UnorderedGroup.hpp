#pragma once
#include <algorithm>
#include <pegium/grammar/UnorderedGroup.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/StepTrace.hpp>

namespace pegium::parser {

template <ParseExpression... Elements>
struct UnorderedGroup final : grammar::UnorderedGroup {
  static_assert(sizeof...(Elements) > 1,
                "An UnorderedGroup shall contains at least 2 elements.");

  constexpr explicit UnorderedGroup(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {}

  constexpr UnorderedGroup(UnorderedGroup &&)noexcept = default;
  constexpr UnorderedGroup(const UnorderedGroup &) = default;
  constexpr UnorderedGroup &operator=(UnorderedGroup &&) noexcept = default;
  constexpr UnorderedGroup &operator=(const UnorderedGroup &) = default;

  constexpr bool parse_rule(ParseState &s) const {
    const auto entry = s.mark();
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |= parse_rule_element(element, s, processed,
                                                 index++)),
             ...);
          },
          _elements);

      if (!anyProcessed)
        break;
    }

    if (std::ranges::all_of(processed, [](bool p) { return p; })) {
      return true;
    }

    s.rewind(entry);
    return false;
  }

  bool recover(RecoverState &recoverState) const {
    detail::stepTraceInc(detail::StepCounter::UnorderedRecoverCalls);
    if (recoverState.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::UnorderedStrictPasses);
      return recover_strict(recoverState);
    }

    const auto entry = recoverState.mark();

    const bool allowInsert = recoverState.allowInsert;
    const bool allowDelete = recoverState.allowDelete;
    recoverState.allowInsert = false;
    recoverState.allowDelete = false;
    detail::stepTraceInc(detail::StepCounter::UnorderedStrictPasses);
    if (recover_strict(recoverState)) {
      recoverState.allowInsert = allowInsert;
      recoverState.allowDelete = allowDelete;
      return true;
    }
    recoverState.rewind(entry);
    recoverState.allowInsert = allowInsert;
    recoverState.allowDelete = allowDelete;

    if (recover_editable(recoverState)) {
      detail::stepTraceInc(detail::StepCounter::UnorderedEditablePasses);
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::UnorderedEditablePasses);
    recoverState.rewind(entry);
    return false;
  }

  constexpr MatchResult parse_terminal(const char *begin,
                                       const char *end) const noexcept {
    MatchResult r = MatchResult::success(begin);
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |=
              parse_terminal_element(element, end, r, processed, index++)),
             ...);
          },
          _elements);

      if (!anyProcessed)
        break;
    }

    return std::ranges::all_of(processed, [](bool p) { return p; })
               ? MatchResult::success(r.offset)
               : MatchResult::failure(r.offset);
  }
  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    return parse_terminal(sv.begin(), sv.end());
  }

  void print(std::ostream &os) const override {
    os << '(';
    bool first = true;
    std::apply(
        [&](auto const &...elems) {
          ((os << (first ? "" : " & "), os << elems, first = false), ...);
        },
        _elements);

    os << ')';
  }

private:
  using ProcessedFlags = std::array<bool, sizeof...(Elements)>;

  template <typename T>
  static constexpr bool parse_rule_element(const T &element,
                                           ParseState &s,
                                           ProcessedFlags &processed,
                                           std::size_t index) {
    if (processed[index])
      return false;

    auto chekpoint = s.mark();
    if (element.parse_rule(s)) {
      processed[index] = true;
      return true;
    }
    s.rewind(chekpoint);
    return false;
  }

  template <typename T>
  static bool parse_recover_element(const T &element, ProcessedFlags &processed,
                                    RecoverState &recoverState,
                                    std::size_t index) {
    if (processed[index])
      return false;

    auto chekpoint = recoverState.mark();
    if (element.recover(recoverState)) {
      processed[index] = true;
      return true;
    }
    recoverState.rewind(chekpoint);
    return false;
  }

  bool recover_impl(RecoverState &recoverState) const {
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |= parse_recover_element(element, processed,
                                                    recoverState, index++)),
             ...);
          },
          _elements);

      if (!anyProcessed)
        break;
    }
    return std::ranges::all_of(processed, [](bool p) { return p; });
  }

  bool recover_strict(RecoverState &recoverState) const {
    const auto mark = recoverState.mark();
    if (!recover_impl(recoverState)) {
      recoverState.rewind(mark);
      return false;
    }
    return true;
  }

  bool recover_editable(RecoverState &recoverState) const {
    return recover_impl(recoverState);
  }

  template <typename T>
  static constexpr bool
  parse_terminal_element(const T &element, const char *end, MatchResult &r,
                         ProcessedFlags &processed,
                         std::size_t index) noexcept {
    if (processed[index])
      return false;

    if (auto result = element.parse_terminal(r.offset, end);
        result.IsValid()) {
      r = result;
      processed[index] = true;
      return true;
    }
    return false;
  }

  std::tuple<Elements...> _elements;

  template <ParseExpression... Rhs>
  friend constexpr auto operator&(UnorderedGroup &&lhs,
                                  UnorderedGroup<Rhs...> &&rhs) {
    return UnorderedGroup<Elements..., ParseExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParseExpression Rhs>
  friend constexpr auto operator&(UnorderedGroup &&lhs, Rhs &&rhs) {
    return UnorderedGroup<Elements..., ParseExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(std::forward<Rhs>(rhs)))};
  }
  template <ParseExpression Lhs>
  friend constexpr auto operator&(Lhs &&lhs, UnorderedGroup &&rhs) {
    return UnorderedGroup<ParseExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(std::forward<Lhs>(lhs)), std::move(rhs._elements))};
  }
};
template <ParseExpression Lhs, ParseExpression Rhs>
constexpr auto operator&(Lhs &&lhs, Rhs &&rhs) {
  return UnorderedGroup<ParseExpressionHolder<Lhs>,
                        ParseExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

} // namespace pegium::parser
