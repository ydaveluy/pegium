#pragma once
#include <optional>
#include <pegium/grammar/DataTypeRule.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/CompletionSupport.hpp>
#include <pegium/parser/DataTypeRuleSupport.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/NodeParseHelpers.hpp>
#include <pegium/parser/ParseMode.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RuleOptions.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <string>
#include <string_view>
namespace pegium::parser {

template <typename T = std::string>
  requires(!std::derived_from<T, AstNode>) && detail::SupportedRuleValueType<T>
struct DataTypeRule final : AbstractRule<grammar::DataTypeRule>,
                            CompletionSkipperProvider {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRule<grammar::DataTypeRule>;
  static constexpr bool isFailureSafe = false;
  using BaseRule::BaseRule;

  template <NonNullableExpression Element, typename... Options>
    requires(sizeof...(Options) > 0)
  constexpr DataTypeRule(std::string_view name, Element &&element,
                         Options &&...options)
      : BaseRule(name, std::forward<Element>(element)) {
    (applyOption(std::forward<Options>(options)), ...);
  }

  DataTypeRule super() const { return *this; }

  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }
  T getRawValue(const CstNodeView &node) const {
    return convertValue(node, nullptr);
  }

  T getRawValue(const CstNodeView &node,
                const ValueBuildContext &context) const {
    return convertValue(node, &context);
  }

  value_variant getValue(const CstNodeView &node,
                         const ValueBuildContext *context = nullptr) const override {
    return detail::toRuleValue(convertValue(node, context));
  }

  [[nodiscard]] const Skipper *
  getCompletionSkipper() const noexcept override {
    return _localSkipper.has_value() ? std::addressof(*_localSkipper) : nullptr;
  }

  using BaseRule::operator=;

private:
  friend struct detail::ParseAccess;

  std::optional<Skipper> _localSkipper;
  std::function<opt::ConversionResult<T>(const CstNodeView &,
                                         const ValueBuildContext *)>
      _value_converter;
  bool _hasCustomValueConverter = false;
  using ValueSupport = detail::DataTypeRuleValueSupport<T>;

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (RecoveryParseModeContext<Context>) {
      if (!ctx.isInRecoveryPhase() && !ctx.hasPendingRecoveryWindows()) {
        return parse_impl(static_cast<TrackedParseContext &>(ctx));
      }
    }
    if constexpr (ExpectParseModeContext<Context>) {
      if (ctx.reachedAnchor()) {
        ctx.addRule(this);
        return true;
      }
    }
    if constexpr (!StrictParseModeContext<Context>) {
      if (ctx.isActiveRecovery(this)) {
        return false;
      }
      auto activeRecoveryGuard = ctx.enterActiveRecovery(this);
      (void)activeRecoveryGuard;
    }

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

  T convertValue(const CstNodeView &node,
                 const ValueBuildContext *context) const {
    return ValueSupport::convert_value(*this, node, context,
                                       _hasCustomValueConverter,
                                       _value_converter);
  }

  template <typename Option> void applyOption(Option &&option) {
    using OptionType = std::remove_cvref_t<Option>;
    if constexpr (opt::IsSkipperOption_v<OptionType>) {
      _localSkipper = std::forward<Option>(option).skipper;
    } else if constexpr (opt::IsConverterOption_v<OptionType>) {
      setValueConverterFromOption(std::forward<Option>(option).converter);
    } else {
      static_assert(opt::detail::DependentFalse_v<OptionType>,
                    "Unsupported option for DataTypeRule. "
                    "Supported options: opt::with_skipper(...), "
                    "opt::with_converter(...).");
    }
  }

  template <typename Converter>
  void setValueConverterFromOption(Converter &&converter) {
    ValueSupport::set_value_converter_from_option(
        _value_converter, _hasCustomValueConverter,
        std::forward<Converter>(converter));
  }
};

} // namespace pegium::parser
