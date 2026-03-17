#pragma once

#include <array>
#include <concepts>
#include <tuple>
#include <utility>

#include <pegium/grammar/Group.hpp>
#include <pegium/parser/CompletionSupport.hpp>
#include <pegium/parser/ExpectFrontier.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/ParseAttempt.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/ParseExpression.hpp>
#include <pegium/parser/RecoveryEditSupport.hpp>
#include <pegium/parser/SkipperBuilder.hpp>
#include <pegium/parser/TextUtils.hpp>
#include <string>
#include <string_view>

namespace pegium::parser {

template <Expression... Elements> struct GroupWithSkipper;

template <Expression... Elements> struct Group : grammar::Group {
  static constexpr bool nullable =
      (... && std::remove_cvref_t<Elements>::nullable);
  static constexpr bool isFailureSafe = false;
  static_assert(sizeof...(Elements) > 1,
                "A Group shall contains at least 2 elements.");

  constexpr explicit Group(std::tuple<Elements...> &&tupleElements)
      : elements{std::move(tupleElements)} {}

protected:
  constexpr explicit Group(const std::tuple<Elements...> &tupleElements)
      : elements{tupleElements} {}

public:
  constexpr Group(Group &&) noexcept = default;
  constexpr Group(const Group &) = default;
  constexpr Group &operator=(Group &&) noexcept = default;
  constexpr Group &operator=(const Group &) = default;

  bool probeRecoverable(RecoveryContext &ctx) const {
    return attempt_fast_probe(ctx, std::get<0>(elements)) ||
           probe_locally_recoverable(std::get<0>(elements), ctx);
  }

  constexpr const char *terminal(const char *begin) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    return terminal_impl(begin);
  }
  constexpr const char *terminal(const std::string &text) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    return terminal(text.c_str());
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<std::tuple<Elements...>>
  auto skip(LocalSkipper &&localSkipper) const & {
    return with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto skip(LocalSkipper &&localSkipper) && {
    return std::move(*this).with_skipper(std::forward<LocalSkipper>(localSkipper));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>))) &&
            std::copy_constructible<std::tuple<Elements...>>
  auto skip(SkipperParts &&...parts) const & {
    return with_skipper(parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <typename... SkipperParts>
    requires((... && (detail::IsHiddenRules_v<SkipperParts> ||
                     detail::IsIgnoredRules_v<SkipperParts>)))
  auto skip(SkipperParts &&...parts) && {
    return std::move(*this).with_skipper(
        parser::skip(std::forward<SkipperParts>(parts)...));
  }

  template <std::convertible_to<Skipper> LocalSkipper>
    requires std::copy_constructible<std::tuple<Elements...>>
  auto with_skipper(LocalSkipper &&localSkipper) const & {
    return GroupWithSkipper<Elements...>{
        elements,
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return GroupWithSkipper<Elements...>{
        std::move(elements),
        static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  const AbstractElement *get(std::size_t elementIndex) const noexcept override {
    if (elementIndex >= sizeof...(Elements))
      return nullptr;
    return get_impl(elementIndex,
                    std::make_index_sequence<sizeof...(Elements)>());
  }

  std::size_t size() const noexcept override { return sizeof...(Elements); }
  constexpr bool isNullable() const noexcept override {
    return nullable;
  }
  std::tuple<Elements...> elements;

protected:
  friend struct detail::ParseAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    return parse_elements<Context, 0>(ctx);
  }

  template <ParseModeContext Context, std::size_t I>
  bool parse_elements(Context &ctx) const {
    if constexpr (I == sizeof...(Elements)) {
      return true;
    } else {
      if constexpr (I > 0) {
        ctx.skip();
      }
      if constexpr (StrictParseModeContext<Context>) {
        if (!parse(std::get<I>(elements), ctx)) {
          return false;
        }
        return parse_elements<Context, I + 1>(ctx);
      } else if constexpr (RecoveryParseModeContext<Context>) {
        if (!ctx.isInRecoveryPhase()) {
          TrackedParseContext &strictCtx = ctx;
          if (!parse(std::get<I>(elements), strictCtx)) {
            return false;
          }
          return parse_elements<Context, I + 1>(ctx);
        }
        if (try_sync_missing_element<I>(ctx)) {
          return parse_elements<Context, I + 1>(ctx);
        }
        if (!parse(std::get<I>(elements), ctx)) {
          return false;
        }
        return parse_elements<Context, I + 1>(ctx);
      } else {
        const auto checkpoint = ctx.mark();
        if (!parse(std::get<I>(elements), ctx)) {
          return false;
        }
        if (!ctx.frontierBlocked()) {
          return parse_elements<Context, I + 1>(ctx);
        }
        if constexpr (std::remove_cvref_t<
                          decltype(std::get<I>(elements))>::nullable) {
          auto currentFrontier = capture_frontier_since(ctx, checkpoint);
          ctx.clearFrontierBlock();
          if (parse_elements<Context, I + 1>(ctx)) {
            const bool tailBlocked = ctx.frontierBlocked();
            merge_captured_frontier(ctx, currentFrontier, !tailBlocked);
            return true;
          }
          ctx.rewind(checkpoint);
          merge_captured_frontier(ctx, currentFrontier, false);
        }
        return true;
      }
    }
  }

  template <std::size_t I>
  bool try_sync_missing_element(RecoveryContext &ctx) const {
    if constexpr (I + 1 >= sizeof...(Elements)) {
      return false;
    } else if constexpr (std::remove_cvref_t<
                             decltype(std::get<I>(elements))>::nullable) {
      return false;
    } else {
      const auto &current = std::get<I>(elements);
      const auto &next = std::get<I + 1>(elements);
      if (current.getKind() != ElementKind::Assignment ||
          !isDelimiterLiteral(next)) {
        return false;
      }
      if (detail::attempt_parse_without_side_effects(ctx, current)) {
        return false;
      }
      if (probe_locally_recoverable(current, ctx)) {
        return false;
      }

      const auto syncCheckpoint = ctx.mark();
      ctx.skip();
      const bool nextMatches = detail::attempt_parse_without_side_effects(
          ctx, next);
      ctx.rewind(syncCheckpoint);
      if (!nextMatches) {
        return false;
      }

      return detail::apply_insert_hidden_recovery_edit(
          ctx, std::addressof(current));
    }
  }

private:
  template <typename E>
  static bool isDelimiterLiteral(const E &element) {
    if constexpr (!std::derived_from<std::remove_cvref_t<E>, grammar::Literal>) {
      return false;
    } else {
      if (element.getKind() != ElementKind::Literal) {
        return false;
      }
      const auto value = element.getValue();
      return !value.empty() &&
             !std::ranges::all_of(value, [](char c) { return isWord(c); });
    }
  }

  template <std::size_t... Is>
  const AbstractElement *get_impl(std::size_t elementIndex,
                                  std::index_sequence<Is...>) const noexcept {
    using AccessorFn = const AbstractElement *(*)(const Group *) noexcept;

    static constexpr std::array<AccessorFn, sizeof...(Elements)> accessors = {
        +[](const Group *self) noexcept -> const AbstractElement * {
          return std::addressof(std::get<Is>(self->elements));
        }...};

    return accessors[elementIndex](this);
  }

  template <std::size_t I = 0>
  constexpr const char *terminal_impl(const char *cursor) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    if constexpr (I == sizeof...(Elements)) {
      return cursor;
    } else {
      const char *matchEnd = std::get<I>(elements).terminal(cursor);
      return matchEnd != nullptr ? terminal_impl<I + 1>(matchEnd)
                                 : nullptr;
    }
  }
};

template <Expression... Elements>
struct GroupWithSkipper final : Group<Elements...>, CompletionSkipperProvider {
  using Base = Group<Elements...>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit GroupWithSkipper(std::tuple<Elements...> &&elements,
                            Skipper localSkipper)
      : Base{std::move(elements)}, _localSkipper{std::move(localSkipper)} {}

  explicit GroupWithSkipper(const std::tuple<Elements...> &elements,
                            Skipper localSkipper)
      : Base{elements}, _localSkipper{std::move(localSkipper)} {}

  GroupWithSkipper(GroupWithSkipper &&) noexcept = default;
  GroupWithSkipper(const GroupWithSkipper &) = default;
  GroupWithSkipper &operator=(GroupWithSkipper &&) noexcept = default;
  GroupWithSkipper &operator=(const GroupWithSkipper &) = default;
  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return std::addressof(_localSkipper);
  }

private:
  friend struct detail::ParseAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    auto localSkipperGuard = ctx.with_skipper(_localSkipper);
    (void)localSkipperGuard;
    return parse(static_cast<const Base &>(*this), ctx);
  }

  Skipper _localSkipper;
};

namespace detail {

template <typename T> struct IsGroupRaw : std::false_type {};
template <Expression... E> struct IsGroupRaw<Group<E...>> : std::true_type {};

template <typename G>
  requires IsGroupRaw<std::remove_cvref_t<G>>::value
constexpr decltype(auto) as_group_tuple(G &&group) {
  return std::forward<G>(group).elements;
}

template <Expression Expr>
  requires(!IsGroupRaw<std::remove_cvref_t<Expr>>::value)
constexpr auto as_group_tuple(Expr &&expr) {
  return std::tuple<ExpressionHolder<Expr>>{std::forward<Expr>(expr)};
}

template <typename... Ts>
constexpr auto make_group(std::tuple<Ts...> &&elements) {
  return Group<Ts...>{std::move(elements)};
}

} // namespace detail

template <Expression Lhs, Expression Rhs>
constexpr auto operator+(Lhs &&lhs, Rhs &&rhs) {
  return detail::make_group(std::tuple_cat(
      detail::as_group_tuple(std::forward<Lhs>(lhs)),
      detail::as_group_tuple(std::forward<Rhs>(rhs))));
}
} // namespace pegium::parser
