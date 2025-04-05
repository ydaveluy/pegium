#pragma once
#include <algorithm>
#include <pegium/grammar/IGrammarElement.hpp>
namespace pegium::grammar {

template <typename... Elements>
  requires(IsGrammarElement<Elements> && ...)
struct UnorderedGroup : IGrammarElement {
  static_assert(sizeof...(Elements) > 1,
                "An UnorderedGroup shall contains at least 2 elements.");
  constexpr ~UnorderedGroup() override = default;
  std::tuple<Elements...> elements;

  constexpr explicit UnorderedGroup(std::tuple<Elements...> &&elems)
      : elements{std::move(elems)} {}

  using ProcessedFlags = std::array<bool, sizeof...(Elements)>;

  template <typename T>
  static constexpr bool
  parse_rule_element(const T &element, std::string_view sv, CstNode &parent,
                     IContext &c, MatchResult &i, ProcessedFlags &processed,
                     std::size_t index) {
    if (processed[index]) {
      return false;
    }

    if (auto len =
            element.parse_rule({sv.data() + i, sv.size() - i}, parent, c);
        success(len)) {
      i += len;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr MatchResult parse_rule(std::string_view sv, CstNode &parent,
                                   IContext &c) const override {
                                    MatchResult i = 0;
    ProcessedFlags processed{};

    while (!std::ranges::all_of(processed, [](bool p) { return p; })) {
      bool anyProcessed = std::apply(
          [&](const auto &...element) {
            std::size_t index = 0;
            return (parse_rule_element(element, sv, parent, c, i, processed,
                                       index++) ||
                    ...);
          },
          elements);

      if (!anyProcessed) {
        break;
      }
    }
    
    return std::ranges::all_of(processed, [](bool p) { return p; })
               ? MatchResult::success(i)
               : MatchResult::failure(i);
  }

  template <typename T>
  static constexpr bool
  parse_terminal_element(const T &element, std::string_view sv, MatchResult &i,
                         ProcessedFlags &processed,
                         std::size_t index) noexcept {
    if (processed[index]) {
      return false;
    }

    if (auto len = element.parse_terminal({sv.data() + i, sv.size() - i});
        success(len)) {
      i += len;
      processed[index] = true;
      return true;
    }
    return false;
  }

  constexpr MatchResult
  parse_terminal(std::string_view sv) const noexcept override {
    MatchResult i = 0;
    ProcessedFlags processed{};

    while (!std::ranges::all_of(processed, [](bool p) { return p; })) {
      bool anyProcessed = std::apply(
          [&](const auto &...element) {
            std::size_t index = 0;
            return (
                parse_terminal_element(element, sv, i, processed, index++) ||
                ...);
          },
          elements);

      if (!anyProcessed) {
        break;
      }
    }
    return std::ranges::all_of(processed, [](bool p) { return p; })
               ? MatchResult::success(i)
               : MatchResult::failure(i);
  }
  void print(std::ostream &os) const override {
    os << '(';
    std::apply([&os](const auto &...args) {
        bool first = true;
        ((os << (first ? "" : " & ") << args, first = false), ...);
    }, elements);
    os << ')';
  }
  constexpr GrammarElementKind getKind() const noexcept override {
    return GrammarElementKind::UnorderedGroup;
  }
};

template <typename... Lhs, typename... Rhs>
  requires(IsGrammarElement<Lhs> && ...) && (IsGrammarElement<Rhs> && ...)
constexpr auto operator&(UnorderedGroup<Lhs...> &&lhs,
                         UnorderedGroup<Rhs...> &&rhs) {
  return UnorderedGroup<GrammarElementType<Lhs>..., GrammarElementType<Rhs>...>{
      std::tuple_cat(std::move(lhs.elements), std::move(rhs.elements))};
}

template <typename... Lhs, typename Rhs>
  requires(IsGrammarElement<Lhs> && ...) && IsGrammarElement<Rhs>
constexpr auto operator&(UnorderedGroup<Lhs...> &&lhs, Rhs &&rhs) {
  return UnorderedGroup<GrammarElementType<Lhs>..., GrammarElementType<Rhs>>{
      std::tuple_cat(std::move(lhs.elements), std::forward_as_tuple(rhs))};
}
template <typename Lhs, typename... Rhs>
  requires IsGrammarElement<Lhs> && (IsGrammarElement<Rhs> && ...)
constexpr auto operator&(Lhs &&lhs, UnorderedGroup<Rhs...> &&rhs) {
  return UnorderedGroup<GrammarElementType<Lhs>, GrammarElementType<Rhs>...>{
      std::tuple_cat(std::forward_as_tuple(lhs), std::move(rhs.elements))};
}

template <typename Lhs, typename Rhs>
  requires IsGrammarElement<Lhs> && IsGrammarElement<Rhs>
constexpr auto operator&(Lhs &&lhs, Rhs &&rhs) {
  return UnorderedGroup<GrammarElementType<Lhs>, GrammarElementType<Rhs>>{
      std::forward_as_tuple(std::forward<Lhs>(lhs), std::forward<Rhs>(rhs))};
}

} // namespace pegium::grammar