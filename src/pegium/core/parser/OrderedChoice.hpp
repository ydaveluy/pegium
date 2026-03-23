#pragma once

/// Parser expression representing prioritized alternatives.

#include <concepts>
#include <array>
#include <limits>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/parser/CompletionSupport.hpp>
#include <pegium/core/parser/CstSearch.hpp>
#include <pegium/core/parser/EditableRecoverySupport.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/ExpectFrontier.hpp>
#include <pegium/core/parser/ParseAttempt.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/RecoveryUtils.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/parser/StepTrace.hpp>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

namespace pegium::parser {

template <Expression... Elements> struct OrderedChoiceWithSkipper;

template <Expression... Elements>
struct OrderedChoice : grammar::OrderedChoice {
  static_assert(sizeof...(Elements) > 1,
                "An OrderedChoice shall contains at least 2 elements.");
  static constexpr bool nullable =
      (... || std::remove_cvref_t<Elements>::nullable);
  static constexpr bool isFailureSafe = true;
  static consteval bool nullable_only_last() {
    constexpr bool flags[] = {std::remove_cvref_t<Elements>::nullable...};
    for (std::size_t i = 0; i + 1 < sizeof...(Elements); ++i)
      if (flags[i])
        return false;
    return true;
  }

  static_assert(nullable_only_last(),
                "OrderedChoice: a nullable alternative must be the last one.");

  constexpr explicit OrderedChoice(std::tuple<Elements...> &&elements)
      : choices{std::move(elements)} {}
  constexpr OrderedChoice(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice(const OrderedChoice &) = default;
  constexpr OrderedChoice &operator=(OrderedChoice &&) noexcept = default;
  constexpr OrderedChoice &operator=(const OrderedChoice &) = default;

private:
  friend struct detail::ParseAccess;
  friend struct detail::ProbeAccess;
  friend struct detail::FastProbeAccess;
  friend struct detail::InitAccess;

public:
  void init_impl(AstReflectionInitContext &ctx) const {
    init_choices<0>(ctx);
  }

private:

  template <StrictParseModeContext Context>
  bool probe_impl(Context &ctx) const {
    return probe_choices(ctx, std::make_index_sequence<sizeof...(Elements)>{});
  }

  template <StrictParseModeContext Context>
  bool fast_probe_impl(Context &ctx) const {
    return fast_probe_choices(ctx,
                              std::make_index_sequence<sizeof...(Elements)>{});
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
      return match_choice(ctx);
    } else if constexpr (RecoveryParseModeContext<Context>) {
      detail::stepTraceInc(detail::StepCounter::ChoiceRecoverCalls);

      const auto entryCheckpoint = ctx.mark();
      PEGIUM_RECOVERY_TRACE("[choice rule] enter offset=", ctx.cursorOffset(),
                            " allowI=", ctx.allowInsert,
                            " allowD=", ctx.allowDelete);

      if (run_no_edit_probe(ctx, entryCheckpoint)) {
        return true;
      }

      if (run_editable_choice(ctx, entryCheckpoint)) {
        return true;
      }

      PEGIUM_RECOVERY_TRACE("[choice rule] fail offset=", ctx.cursorOffset());
      return false;
    } else {
      const auto base = ctx.mark();
      std::array<BranchResult, sizeof...(Elements)> branches{};
      collect_expect_results(ctx, base, branches,
                             std::make_index_sequence<sizeof...(Elements)>{});

      std::optional<std::size_t> bestIndex;
      for (std::size_t index = 0; index < branches.size(); ++index) {
        const auto &candidate = branches[index];
        if (!candidate.matched) {
          continue;
        }
        if (!bestIndex.has_value()) {
          bestIndex = index;
          continue;
        }
        const auto &best = branches[*bestIndex];
        if (candidate.cursor > best.cursor ||
            (candidate.cursor == best.cursor &&
             candidate.blocked != best.blocked && !candidate.blocked) ||
            (candidate.cursor == best.cursor &&
             candidate.blocked == best.blocked &&
             candidate.editCost < best.editCost)) {
          bestIndex = index;
        }
      }

      if (!bestIndex.has_value()) {
        ctx.rewind(base);
        return false;
      }

      ctx.rewind(base);
      if (!replay_expect_branch_by_index(ctx, *bestIndex)) {
        return false;
      }

      const auto &best = branches[*bestIndex];
      for (std::size_t index = 0; index < branches.size(); ++index) {
        if (index == *bestIndex) {
          continue;
        }
        const auto &candidate = branches[index];
        if (!candidate.matched || candidate.cursor != best.cursor ||
            candidate.editCost != best.editCost) {
          continue;
        }
        ctx.mergeFrontier(candidate.frontier);
      }
      if (!best.blocked) {
        ctx.clearFrontierBlock();
      }
      return true;
    }
  }

public:
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
    return OrderedChoiceWithSkipper<Elements...>{
        *this, static_cast<Skipper>(std::forward<LocalSkipper>(localSkipper))};
  }

  template <std::convertible_to<Skipper> LocalSkipper>
  auto with_skipper(LocalSkipper &&localSkipper) && {
    return OrderedChoiceWithSkipper<Elements...>{
        std::move(*this),
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

private:
  using BranchResult = ExpectBranchResult;
  using EditableRecoveryCandidate = detail::EditableRecoveryCandidate;

  template <typename Checkpoint>
  bool run_no_edit_probe(RecoveryContext &ctx,
                         const Checkpoint &entryCheckpoint) const {
    if (match_choice_no_edits(ctx)) {
      detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
      PEGIUM_RECOVERY_TRACE("[choice rule] strict success offset=",
                            ctx.cursorOffset());
      return true;
    }
    detail::stepTraceInc(detail::StepCounter::ChoiceStrictPasses);
    ctx.rewind(entryCheckpoint);
    return false;
  }

  bool run_delete_retry_choice(RecoveryContext &ctx) const {
    return detail::recover_by_delete_retry(ctx, [this, &ctx]() {
      ctx.skip();
      if (!match_choice(ctx)) {
        return false;
      }
      PEGIUM_RECOVERY_TRACE("[choice rule] editable delete-retry success offset=",
                            ctx.cursorOffset());
      return true;
    });
  }

  template <typename Checkpoint>
  bool run_editable_choice(RecoveryContext &ctx,
                           const Checkpoint &entryCheckpoint) const {
    const auto parseStartOffset = ctx.cursorOffset();
    const auto baseEditCost = ctx.currentEditCost();
    const auto baseEditCount = ctx.currentEditCount();
    const auto baseRecoveryEditCount = ctx.recoveryEditCount();

    const auto editableCandidate = detail::evaluate_editable_recovery_candidate(
        ctx, entryCheckpoint, baseEditCost, baseEditCount, baseRecoveryEditCount,
        [this, &ctx]() { return match_choice(ctx); });

    EditableRecoveryCandidate deleteRetryCandidate;
    if (ctx.isInRecoveryPhase()) {
      deleteRetryCandidate = detail::evaluate_editable_recovery_candidate(
          ctx, entryCheckpoint, baseEditCost, baseEditCount,
          baseRecoveryEditCount,
          [this, &ctx]() { return run_delete_retry_choice(ctx); });
    }

    detail::stepTraceInc(detail::StepCounter::ChoiceEditablePasses);
    if (!editableCandidate.matched && !deleteRetryCandidate.matched) {
      return false;
    }

    if (detail::prefer_efficiency_weighted_delete_retry_candidate(
            deleteRetryCandidate, editableCandidate, parseStartOffset)) {
      return run_delete_retry_choice(ctx);
    }

    if (match_choice(ctx)) {
      PEGIUM_RECOVERY_TRACE("[choice rule] editable success offset=",
                            ctx.cursorOffset());
      return true;
    }
    ctx.rewind(entryCheckpoint);
    return false;
  }

public:
  std::tuple<Elements...> choices;

private:
  template <std::size_t I>
  void init_choices(AstReflectionInitContext &ctx) const {
    if constexpr (I < sizeof...(Elements)) {
      parser::init(std::get<I>(choices), ctx);
      init_choices<I + 1>(ctx);
    }
  }


  template <std::size_t... Is>
  const AbstractElement *get_impl(std::size_t elementIndex,
                                  std::index_sequence<Is...>) const noexcept {
    using AccessorFn =
        const AbstractElement *(*)(const OrderedChoice *) noexcept;

    static constexpr std::array<AccessorFn, sizeof...(Elements)> accessors = {
        +[](const OrderedChoice *self) noexcept -> const AbstractElement * {
          return std::addressof(std::get<Is>(self->choices));
        }...};

    return accessors[elementIndex](this);
  }

  template <StrictParseModeContext Context, std::size_t... Is>
  bool probe_choices(Context &ctx, std::index_sequence<Is...>) const {
    return (... || parser::probe(std::get<Is>(choices), ctx));
  }

  template <StrictParseModeContext Context, std::size_t... Is>
  bool fast_probe_choices(Context &ctx,
                          std::index_sequence<Is...>) const {
    return (... || parser::attempt_fast_probe(ctx, std::get<Is>(choices)));
  }

  template <std::size_t I = 0>
  constexpr const char *terminal_impl(const char *inputBegin) const noexcept
    requires(... && TerminalCapableExpression<Elements>)
  {
    if constexpr (I == sizeof...(Elements)) {
      return nullptr;
    } else {
      const char *matchEnd = std::get<I>(choices).terminal(inputBegin);
      return matchEnd != nullptr ? matchEnd
                                 : terminal_impl<I + 1>(inputBegin);
    }
  }

  template <std::size_t I = 0, StrictParseModeContext Context>
  bool match_choice(Context &ctx) const {
    if (attempt_parse_strict(ctx, std::get<I>(choices))) {
      return true;
    }
    if constexpr (I + 1 == sizeof...(Elements)) {
      return false;
    } else {
      return match_choice<I + 1>(ctx);
    }
  }

  template <std::size_t I = 0>
  bool match_choice(RecoveryContext &ctx) const {
    if (attempt_parse_editable(ctx, std::get<I>(choices))) {
      return true;
    }
    if constexpr (I + 1 == sizeof...(Elements)) {
      return false;
    } else {
      return match_choice<I + 1>(ctx);
    }
  }

  template <std::size_t I = 0>
  bool match_choice_no_edits(RecoveryContext &ctx) const {
    if (attempt_parse_no_edits(ctx, std::get<I>(choices))) {
      return true;
    }
    if constexpr (I + 1 == sizeof...(Elements)) {
      return false;
    } else {
      return match_choice_no_edits<I + 1>(ctx);
    }
  }

  template <std::size_t... Is>
  void collect_expect_results(ExpectContext &ctx,
                              const ExpectContext::Checkpoint &base,
                              std::array<BranchResult, sizeof...(Elements)> &branches,
                              std::index_sequence<Is...>) const {
    (collect_expect_result<Is>(ctx, base, branches[Is]), ...);
  }

  template <std::size_t I>
  void collect_expect_result(ExpectContext &ctx,
                             const ExpectContext::Checkpoint &base,
                             BranchResult &result) const {
    collect_expect_branch(ctx, base, std::get<I>(choices), result);
  }

  template <std::size_t I = 0>
  bool replay_expect_branch_by_index(ExpectContext &ctx,
                                     std::size_t bestIndex) const {
    if constexpr (I == sizeof...(Elements)) {
      return false;
    } else {
      if (bestIndex == I) {
        return parser::replay_expect_branch(ctx, std::get<I>(choices));
      }
      return replay_expect_branch_by_index<I + 1>(ctx, bestIndex);
    }
  }

};

template <Expression... Elements>
struct OrderedChoiceWithSkipper final : OrderedChoice<Elements...>,
                                        CompletionSkipperProvider {
  using Base = OrderedChoice<Elements...>;
  static constexpr bool nullable = Base::nullable;
  static constexpr bool isFailureSafe = Base::isFailureSafe;

  explicit OrderedChoiceWithSkipper(const Base &base, Skipper localSkipper)
      : Base(base), _localSkipper(std::move(localSkipper)) {}
  explicit OrderedChoiceWithSkipper(Base &&base, Skipper localSkipper)
      : Base(std::move(base)), _localSkipper(std::move(localSkipper)) {}

  OrderedChoiceWithSkipper(OrderedChoiceWithSkipper &&) noexcept = default;
  OrderedChoiceWithSkipper(const OrderedChoiceWithSkipper &) = default;
  OrderedChoiceWithSkipper &
  operator=(OrderedChoiceWithSkipper &&) noexcept = default;
  OrderedChoiceWithSkipper &
  operator=(const OrderedChoiceWithSkipper &) = default;
  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return std::addressof(_localSkipper);
  }

private:
  friend struct detail::ParseAccess;
  friend struct detail::InitAccess;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    auto localSkipperGuard = ctx.with_skipper(_localSkipper);
    (void)localSkipperGuard;
    return parse(static_cast<const Base &>(*this), ctx);
  }

  void init_impl(AstReflectionInitContext &ctx) const {
    static_cast<const Base &>(*this).init_impl(ctx);
  }

  Skipper _localSkipper;
};

namespace detail {

template <typename T> struct IsOrderedChoiceRaw : std::false_type {};

template <typename... E>
struct IsOrderedChoiceRaw<OrderedChoice<E...>> : std::true_type {};

template <typename... E>
struct IsOrderedChoiceRaw<OrderedChoiceWithSkipper<E...>> : std::true_type {};

template <typename T> struct IsOrderedChoiceFlattenRaw : std::false_type {};

template <typename... E>
struct IsOrderedChoiceFlattenRaw<OrderedChoice<E...>> : std::true_type {};

template <typename C>
  requires IsOrderedChoiceFlattenRaw<std::remove_cvref_t<C>>::value
constexpr decltype(auto) as_choice_tuple(C &&choice) {
  return std::forward<C>(choice).choices;
}

template <Expression Expr>
  requires(!IsOrderedChoiceFlattenRaw<std::remove_cvref_t<Expr>>::value)
constexpr auto as_choice_tuple(Expr &&expr) {
  return std::tuple<ExpressionHolder<Expr>>{std::forward<Expr>(expr)};
}

template <typename... Ts>
constexpr auto make_ordered_choice(std::tuple<Ts...> &&elements) {
  return OrderedChoice<Ts...>{std::move(elements)};
}

} // namespace detail

template <Expression Lhs, Expression Rhs>
constexpr auto operator|(Lhs &&lhs, Rhs &&rhs) {
  return detail::make_ordered_choice(std::tuple_cat(
      detail::as_choice_tuple(std::forward<Lhs>(lhs)),
      detail::as_choice_tuple(std::forward<Rhs>(rhs))));
}

template <typename T>
struct IsOrderedChoice : detail::IsOrderedChoiceRaw<std::remove_cvref_t<T>> {};

} // namespace pegium::parser
