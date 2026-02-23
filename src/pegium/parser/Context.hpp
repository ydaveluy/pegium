

#pragma once
#include <cassert>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/TerminalRule.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <string_view>
#include <tuple>

namespace pegium::parser {

template <typename HiddenTuple, typename IgnoredTuple> struct Context;



template <ParseExpression... Hidden, ParseExpression... Ignored>
struct Context<std::tuple<Hidden &...>, std::tuple<Ignored &...>> final {

  template <typename... H, typename... I>
  Context(std::tuple<H &...> &&hiddens, std::tuple<I &...> &&ignored)
      : _hidden{std::forward<std::tuple<H &...>>(hiddens)},
        _ignored{std::forward<std::tuple<I &...>>(ignored)} {}

  [[nodiscard]] const char *skipHiddenNodes(const char *begin, const char *end,
                                            CstBuilder &builder) const noexcept {
    const char *cursor = begin;

    while (true) {
      while (parse_one(_ignored, end, cursor)) {
      }

      const char *const hidden_start = cursor;
      const grammar::AbstractElement *hidden_rule = nullptr;
      if (!parse_one_hidden(_hidden, end, cursor, hidden_rule)) {
        return cursor;
      }

      builder.leaf(hidden_start, cursor, hidden_rule, true);
    }
  }

  [[nodiscard]] MatchResult skipHiddenNodes(std::string_view sv,
                                            CstBuilder &builder) const noexcept {
    return MatchResult::success(skipHiddenNodes(sv.begin(), sv.end(), builder));
  }

  [[nodiscard]] bool
  canForceInsert(const grammar::AbstractElement *element, const char *,
                 const char *) const noexcept {
    if (element == nullptr) {
      return false;
    }

    if (element->getKind() == grammar::ElementKind::TerminalRule) {
      return true;
    }
    if (element->getKind() != grammar::ElementKind::Literal) {
      return false;
    }

    const auto literal =
        static_cast<const grammar::Literal *>(element)->getValue();
    if (literal.size() != 1) {
      return false;
    }

    const char c = literal[0];
    return isSyncPunctuation(c);
  }
  [[nodiscard]] operator Skipper() const noexcept {
    return Skipper::from(*this);
  }

private:
  template <typename Element>
  [[nodiscard]] static bool parse_one_element(const Element &element,
                                              const char *end,
                                              const char *&cursor) noexcept {
    if (cursor == end) [[unlikely]] {
      return false;
    }

    const char *const start = cursor;
    const auto r = element.terminal(start, end);
    if (r.IsValid() && r.offset > start) {
      cursor = r.offset;
      return true;
    }
    return false;
  }

  template <typename Tuple, std::size_t... I>
  [[nodiscard]] static bool parse_one_impl(const Tuple &tuple, const char *end,
                                           const char *&cursor,
                                           std::index_sequence<I...>) noexcept {
    return (parse_one_element(std::get<I>(tuple), end, cursor) || ...);
  }

  template <typename Tuple>
  [[nodiscard]] static bool parse_one(const Tuple &tuple, const char *end,
                                      const char *&cursor) noexcept {
    constexpr std::size_t size = std::tuple_size_v<Tuple>;
    if constexpr (size == 0) {
      (void)tuple;
      (void)end;
      (void)cursor;
      return false;
    } else if constexpr (size == 1) {
      return parse_one_element(std::get<0>(tuple), end, cursor);
    } else if constexpr (size == 2) {
      return parse_one_element(std::get<0>(tuple), end, cursor) ||
             parse_one_element(std::get<1>(tuple), end, cursor);
    } else {
      return parse_one_impl(tuple, end, cursor,
                            std::make_index_sequence<size>{});
    }
  }

  template <typename Element>
  [[nodiscard]] static bool
  parse_one_hidden_element(const Element &element, const char *end,
                           const char *&cursor,
                           const grammar::AbstractElement *&matched) noexcept {
    if (parse_one_element(element, end, cursor)) {
      matched = std::addressof(element);
      return true;
    }
    return false;
  }

  template <typename Tuple, std::size_t... I>
  [[nodiscard]] static bool
  parse_one_hidden_impl(const Tuple &tuple, const char *end,
                        const char *&cursor,
                        const grammar::AbstractElement *&matched,
                        std::index_sequence<I...>) noexcept {
    return (parse_one_hidden_element(std::get<I>(tuple), end, cursor,
                                     matched) ||
            ...);
  }

  template <typename Tuple>
  [[nodiscard]] static bool
  parse_one_hidden(const Tuple &tuple, const char *end, const char *&cursor,
                   const grammar::AbstractElement *&matched) noexcept {
    constexpr std::size_t size = std::tuple_size_v<Tuple>;
    if constexpr (size == 0) {
      (void)tuple;
      (void)end;
      (void)cursor;
      (void)matched;
      return false;
    } else if constexpr (size == 1) {
      return parse_one_hidden_element(std::get<0>(tuple), end, cursor,
                                      matched);
    } else if constexpr (size == 2) {
      return parse_one_hidden_element(std::get<0>(tuple), end, cursor,
                                      matched) ||
             parse_one_hidden_element(std::get<1>(tuple), end, cursor,
                                      matched);
    } else {
      return parse_one_hidden_impl(tuple, end, cursor, matched,
                                   std::make_index_sequence<size>{});
    }
  }

  std::tuple<Hidden &...> _hidden;
  std::tuple<Ignored &...> _ignored;

  [[nodiscard]] static constexpr bool isSyncPunctuation(char c) noexcept {
    return c == ')' || c == ']' || c == '}' || c == ',' || c == ';';
  }
};
template <typename HiddenTuple = std::tuple<>,
          typename IgnoredTuple = std::tuple<>>
struct SkipperBuilder {
  explicit SkipperBuilder(HiddenTuple &&hiddens = std::tie(),
                          IgnoredTuple &&ignored = std::tie())
      : _hidden{std::move(hiddens)}, _ignored{std::move(ignored)} {}

  template <ParseExpression... Ignored>
    requires(detail::IsTerminalRule_v<Ignored> && ...) &&
            (std::tuple_size<IgnoredTuple>::value == 0)
  SkipperBuilder<HiddenTuple, std::tuple<Ignored &...>>
  ignore(Ignored &&...ignored) {
    return SkipperBuilder<HiddenTuple, std::tuple<Ignored &...>>{
        std::move(_hidden),
        std::tuple<Ignored &...>(std::forward<Ignored>(ignored)...)};
  }

  template <ParseExpression... Hidden>
    requires(detail::IsTerminalRule_v<Hidden> && ...) &&
            (std::tuple_size<HiddenTuple>::value == 0)
  SkipperBuilder<std::tuple<Hidden &...>, IgnoredTuple>
  hide(Hidden &&...hidden) {
    return SkipperBuilder<std::tuple<Hidden &...>, IgnoredTuple>{
        std::tuple<Hidden &...>(std::forward<Hidden>(hidden)...),
        std::move(_ignored)};
  }

  auto build() {
    using ContextType = Context<HiddenTuple, IgnoredTuple>;
    return Skipper::owning(
        ContextType{std::move(_hidden), std::move(_ignored)});
  }

private:
  HiddenTuple _hidden;
  IgnoredTuple _ignored;
};
SkipperBuilder() -> SkipperBuilder<>;

template <ParseExpression... H, ParseExpression... I>
Context(std::tuple<H &...> &&, std::tuple<I &...> &&)
    -> Context<std::tuple<H &...>, std::tuple<I &...>>;

} // namespace pegium::parser
