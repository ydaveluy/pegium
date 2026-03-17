#pragma once
#include <concepts>
#include <optional>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/CompletionSupport.hpp>
#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/NodeParseHelpers.hpp>
#include <pegium/parser/Parser.hpp>
#include <pegium/parser/ParserRuleSupport.hpp>
#include <pegium/parser/ParseAttemptRanking.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/RuleOptions.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <string_view>

namespace pegium::parser {

template <typename T>
  requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRule<grammar::ParserRule>,
                          CompletionSkipperProvider {
  using type = T;
  using BaseRule = AbstractRule<grammar::ParserRule>;
  static constexpr bool isFailureSafe = false;
  using BaseRule::BaseRule;

  template <NonNullableExpression Element, typename... Options>
    requires(sizeof...(Options) > 0)
  constexpr ParserRule(std::string_view name, Element &&element,
                       Options &&...options)
      : BaseRule(name, std::forward<Element>(element)) {
    (applyOption(std::forward<Options>(options)), ...);
  }

  ParserRule super() const { return *this; }

  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  std::unique_ptr<AstNode>
  getValue(const CstNodeView &node,
           const ValueBuildContext &context) const override {
    return getRawValue(node, context);
  }

  std::unique_ptr<AstNode> getValue(const CstNodeView &node) const {
    return getRawValue(node, ValueBuildContext{});
  }

  std::unique_ptr<AstNode>
  getValue(const CstNodeView &node,
           std::vector<ReferenceHandle> &references) const {
    return getRawValue(node, ValueBuildContext{.references = &references});
  }

  std::unique_ptr<T> getRawValue(const CstNodeView &node,
                                 std::vector<ReferenceHandle> &references) const {
    return getRawValue(node, ValueBuildContext{.references = &references});
  }

  std::unique_ptr<T> getRawValue(const CstNodeView &node,
                                 const ValueBuildContext &context) const {
    return detail::ParserRuleBuildSupport<T>::get_raw_value(node, context);
  }

  std::unique_ptr<T> getRawValue(const CstNodeView &node) const {
    return getRawValue(node, ValueBuildContext{});
  }

  bool rule(ParseContext &ctx) const override { return parse_impl(ctx); }
  bool rule(TrackedParseContext &ctx) const override { return parse_impl(ctx); }
  bool recover(RecoveryContext &ctx) const override { return parse_impl(ctx); }
  bool expect(ExpectContext &ctx) const override { return parse_impl(ctx); }
  bool probeRecoverable(RecoveryContext &ctx) const {
    if (!_wrapper.has_recovery_probe()) {
      return false;
    }
    if (_localSkipper.has_value()) {
      auto localSkipperGuard = ctx.with_skipper(*_localSkipper);
      (void)localSkipperGuard;
      return _wrapper.probe_recoverable(ctx);
    }
    return _wrapper.probe_recoverable(ctx);
  }
  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return _localSkipper.has_value() ? std::addressof(*_localSkipper) : nullptr;
  }
  using BaseRule::operator=;

private:
  friend struct detail::ParseAccess;

  std::optional<Skipper> _localSkipper;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      PEGIUM_RECOVERY_TRACE("[rule rule] enter ", getName(),
                            " offset=", ctx.cursorOffset());
      const auto nodeStartCheckpoint = ctx.enter();
      bool matched = false;
      if (_localSkipper.has_value()) {
        auto localSkipperGuard = ctx.with_skipper(*_localSkipper);
        (void)localSkipperGuard;
        matched = parse(_wrapper, ctx);
      } else {
        matched = parse(_wrapper, ctx);
      }
      if (!matched) {
        PEGIUM_RECOVERY_TRACE("[rule rule] fail ", getName(),
                              " offset=", ctx.cursorOffset());
        return false;
      }
      ctx.exit(nodeStartCheckpoint, this);
      PEGIUM_RECOVERY_TRACE("[rule rule] ok ", getName(),
                            " offset=", ctx.cursorOffset());
      return true;
    } else if constexpr (RecoveryParseModeContext<Context>) {
      if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows()) {
        return parse_impl(static_cast<TrackedParseContext &>(ctx));
      }
      if (ctx.isActiveRecovery(this)) {
        PEGIUM_RECOVERY_TRACE("[rule recover] recursive same-offset bail ",
                              getName(), " offset=", ctx.cursorOffset());
        return false;
      }
      auto activeRecoveryGuard = ctx.enterActiveRecovery(this);
      (void)activeRecoveryGuard;
      PEGIUM_RECOVERY_TRACE(
          "[rule recover] enter ", getName(), " offset=", ctx.cursorOffset(),
          " allowI=", ctx.allowInsert, " allowD=", ctx.allowDelete);
      const auto nodeStartCheckpoint = ctx.enter();
      bool matched = false;
      if (_localSkipper.has_value()) {
        auto localSkipperGuard = ctx.with_skipper(*_localSkipper);
        (void)localSkipperGuard;
        matched = parse(_wrapper, ctx);
      } else {
        matched = parse(_wrapper, ctx);
      }
      if (!matched) {
        if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows() &&
            ctx.allowTopLevelPartialSuccess &&
            ctx.completedRecoveryWindowCount() > 0 &&
            ctx.activeRecoveryDepth() == 1) {
          ctx.exit(nodeStartCheckpoint, this);
          return true;
        }
        PEGIUM_RECOVERY_TRACE("[rule recover] fail ", getName(),
                              " offset=", ctx.cursorOffset());
        return false;
      }
      PEGIUM_RECOVERY_TRACE("[rule recover] ok ", getName(),
                            " offset=", ctx.cursorOffset(),
                            " hadEdits=", ctx.hasHadEdits());
      ctx.exit(nodeStartCheckpoint, this);
      return true;
    } else {
      if (ctx.isActiveRecovery(this)) {
        return false;
      }
      auto activeRecoveryGuard = ctx.enterActiveRecovery(this);
      (void)activeRecoveryGuard;
      auto ruleGuard = ctx.with_rule(this);
      (void)ruleGuard;
      const auto nodeStartCheckpoint = ctx.enter();
      bool matched = false;
      if (_localSkipper.has_value()) {
        auto localSkipperGuard = ctx.with_skipper(*_localSkipper);
        (void)localSkipperGuard;
        matched = parse(_wrapper, ctx);
      } else {
        matched = parse(_wrapper, ctx);
      }
      if (!matched) {
        return false;
      }
      ctx.exit(nodeStartCheckpoint, this);
      return true;
    }
  }

  template <typename Option> void applyOption(Option &&option) {
    using OptionType = std::remove_cvref_t<Option>;
    if constexpr (opt::IsSkipperOption_v<OptionType>) {
      _localSkipper = std::forward<Option>(option).skipper;
    } else {
      static_assert(opt::detail::DependentFalse_v<OptionType>,
                    "Unsupported option for ParserRule. "
                    "Supported option: opt::with_skipper(...).");
    }
  }
};

namespace detail {

template <typename T> struct IsParserRule : std::false_type {};

template <typename T> struct IsParserRule<ParserRule<T>> : std::true_type {};

} // namespace detail

template <typename T>
concept IsParserRule = detail::IsParserRule<std::remove_cvref_t<T>>::value;

} // namespace pegium::parser
