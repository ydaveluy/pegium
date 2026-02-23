#pragma once
#include <algorithm>
#include <pegium/grammar/UnorderedGroup.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/StepTrace.hpp>

namespace pegium::parser {

template <ParseExpression... Elements>
struct UnorderedGroup final : grammar::UnorderedGroup {
  static constexpr bool nullable = false;
  static_assert(sizeof...(Elements) > 1,
                "An UnorderedGroup shall contains at least 2 elements.");
  static_assert(
      (!std::remove_cvref_t<Elements>::nullable && ...),
      "An UnorderedGroup cannot be initialized with nullable elements.");

  constexpr explicit UnorderedGroup(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {}

  constexpr UnorderedGroup(UnorderedGroup &&) noexcept = default;
  constexpr UnorderedGroup(const UnorderedGroup &) = default;
  constexpr UnorderedGroup &operator=(UnorderedGroup &&) noexcept = default;
  constexpr UnorderedGroup &operator=(const UnorderedGroup &) = default;

  bool rule(ParseContext &ctx) const {
    detail::stepTraceInc(detail::StepCounter::UnorderedRecoverCalls);
    if (ctx.isStrictNoEditMode()) {
      detail::stepTraceInc(detail::StepCounter::UnorderedStrictPasses);
      return rule_strict(ctx);
    }

    const auto entry = ctx.mark();

    const bool allowInsert = ctx.allowInsert;
    const bool allowDelete = ctx.allowDelete;
    ctx.allowInsert = false;
    ctx.allowDelete = false;
    detail::stepTraceInc(detail::StepCounter::UnorderedStrictPasses);
    if (rule_strict(ctx)) {
      ctx.allowInsert = allowInsert;
      ctx.allowDelete = allowDelete;
      return true;
    }
    ctx.rewind(entry);
    ctx.allowInsert = allowInsert;
    ctx.allowDelete = allowDelete;

    if (rule_editable(ctx)) {
      detail::stepTraceInc(detail::StepCounter::UnorderedEditablePasses);
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::UnorderedEditablePasses);
    ctx.rewind(entry);
    return false;
  }

  constexpr MatchResult terminal(const char *begin,
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
  constexpr MatchResult terminal(std::string_view sv) const noexcept {
    return terminal(sv.begin(), sv.end());
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
  static bool parse_rule_element(const T &element, ProcessedFlags &processed,
                                    ParseContext &ctx,
                                    std::size_t index) {
    if (processed[index])
      return false;

    auto chekpoint = ctx.mark();
    if (element.rule(ctx)) {
      processed[index] = true;
      return true;
    }
    ctx.rewind(chekpoint);
    return false;
  }

  bool rule_impl(ParseContext &ctx) const {
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |=
              parse_rule_element(element, processed, ctx, index++)),
             ...);
          },
          _elements);

      if (!anyProcessed)
        break;
    }
    return std::ranges::all_of(processed, [](bool p) { return p; });
  }

  bool rule_strict(ParseContext &ctx) const {
    const auto mark = ctx.mark();
    if (!rule_impl(ctx)) {
      ctx.rewind(mark);
      return false;
    }
    return true;
  }

  bool rule_editable(ParseContext &ctx) const {
    return rule_impl(ctx);
  }

  template <typename T>
  static constexpr bool parse_terminal_element(const T &element,
                                               const char *end, MatchResult &r,
                                               ProcessedFlags &processed,
                                               std::size_t index) noexcept {
    if (processed[index])
      return false;

    if (auto result = element.terminal(r.offset, end); result.IsValid()) {
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
        std::tuple_cat(std::move(lhs._elements),
                       std::forward_as_tuple(std::forward<Rhs>(rhs)))};
  }
  template <ParseExpression Lhs>
  friend constexpr auto operator&(Lhs &&lhs, UnorderedGroup &&rhs) {
    return UnorderedGroup<ParseExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(std::forward<Lhs>(lhs)),
                       std::move(rhs._elements))};
  }
};
template <ParseExpression Lhs, ParseExpression Rhs>
constexpr auto operator&(Lhs &&lhs, Rhs &&rhs) {
  return UnorderedGroup<ParseExpressionHolder<Lhs>, ParseExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

} // namespace pegium::parser
