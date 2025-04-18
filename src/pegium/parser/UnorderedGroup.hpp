#pragma once
#include <algorithm>
#include <pegium/grammar/UnorderedGroup.hpp>

#include <pegium/parser/AbstractElement.hpp>

namespace pegium::parser {

template <ParserExpression... Elements>
struct UnorderedGroup final : grammar::UnorderedGroup {
  static_assert(sizeof...(Elements) > 1,
                "An UnorderedGroup shall contains at least 2 elements.");
  // constexpr ~UnorderedGroup() override = default;

  constexpr explicit UnorderedGroup(std::tuple<Elements...> &&elems)
      : _elements{std::move(elems)} {}

  constexpr UnorderedGroup(UnorderedGroup &&) = default;
  constexpr UnorderedGroup(const UnorderedGroup &) = default;
  constexpr UnorderedGroup &operator=(UnorderedGroup &&) = default;
  constexpr UnorderedGroup &operator=(const UnorderedGroup &) = default;

  using ProcessedFlags = std::array<bool, sizeof...(Elements)>;

  template <typename T>
  static constexpr bool
  parse_rule_element(const T &element, std::string_view sv, CstNode &parent,
                     IContext &c, MatchResult &r, ProcessedFlags &processed,
                     std::size_t index) {
    if (processed[index])
      return false;

    if (auto result = element.parse_rule({r.offset, sv.end()}, parent, c)) {
      r = result;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const {
    MatchResult r = MatchResult::success(sv.begin());
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |= parse_rule_element(element, sv, parent, c, r,
                                                 processed, index++)),
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

  template <typename T>
  static constexpr bool
  parse_terminal_element(const T &element, std::string_view sv, MatchResult &r,
                         ProcessedFlags &processed,
                         std::size_t index) noexcept {
    if (processed[index])
      return false;

    if (auto result = element.parse_terminal({r.offset, sv.end()});
        success(result)) {
      r = result;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr MatchResult parse_terminal(std::string_view sv) const noexcept {
    MatchResult r = MatchResult::success(sv.begin());
    ProcessedFlags processed{};

    while (true) {
      bool anyProcessed = false;
      std::size_t index = 0;
      std::apply(
          [&](const auto &...element) {
            ((anyProcessed |=
              parse_terminal_element(element, sv, r, processed, index++)),
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
  std::tuple<Elements...> _elements;

  template <ParserExpression... Rhs>
  friend constexpr auto operator&(UnorderedGroup &&lhs,
                                  UnorderedGroup<Rhs...> &&rhs) {
    return UnorderedGroup<Elements..., ParserExpressionHolder<Rhs>...>{
        std::tuple_cat(std::move(lhs._elements), std::move(rhs._elements))};
  }

  template <ParserExpression Rhs>
  friend constexpr auto operator&(UnorderedGroup &&lhs, Rhs &&rhs) {
    return UnorderedGroup<Elements..., ParserExpressionHolder<Rhs>>{
        std::tuple_cat(std::move(lhs._elements), std::forward_as_tuple(rhs))};
  }
  template <ParserExpression Lhs>
  friend constexpr auto operator&(Lhs &&lhs, UnorderedGroup &&rhs) {
    return UnorderedGroup<ParserExpressionHolder<Lhs>, Elements...>{
        std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs._elements))};
  }
};
template <ParserExpression Lhs, ParserExpression Rhs>
constexpr auto operator&(Lhs &&lhs, Rhs &&rhs) {
  return UnorderedGroup<ParserExpressionHolder<Lhs>,
                        ParserExpressionHolder<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

} // namespace pegium::parser