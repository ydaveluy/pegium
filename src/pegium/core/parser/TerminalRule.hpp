#pragma once

/// Parser rule template producing terminal scalar values.

#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <functional>
#include <pegium/core/parser/AbstractRule.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/RuleOptions.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/RuleValue.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>

namespace pegium::parser {

template <typename T = std::string>
  requires detail::SupportedRuleValueType<T>
struct TerminalRule final : AbstractRule<grammar::TerminalRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRule<grammar::TerminalRule>;
  static constexpr bool isFailureSafe = true;

  template <NonNullableTerminalCapableExpression Element>
  constexpr TerminalRule(std::string_view name, Element &&element)
      : BaseRule(name, std::forward<Element>(element)) {}

  template <NonNullableTerminalCapableExpression Element, typename... Options>
    requires(sizeof...(Options) > 0)
  constexpr TerminalRule(std::string_view name, Element &&element,
                         Options &&...options)
      : BaseRule(name, std::forward<Element>(element)) {
    (applyOption(std::forward<Options>(options)), ...);
  }

  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  const char *terminal(const char *begin) const noexcept {
    assert(this->_wrapper.has_terminal() &&
           "TerminalRule requires a terminal-capable wrapped expression");
    return this->_wrapper.try_terminal(begin);
  }

  const char *terminal(const std::string &text) const noexcept {
    return terminal(text.c_str());
  }

  TerminalRule super() const { return *this; }
  inline T getRawValue(const CstNodeView &node) const {
    return convertValue(node, nullptr);
  }

  inline T getRawValue(const CstNodeView &node,
                       const ValueBuildContext &context) const {
    return convertValue(node, &context);
  }

  value_variant getValue(const CstNodeView &node,
                         const ValueBuildContext *context = nullptr) const override {
    return detail::toRuleValue(convertValue(node, context));
  }

  void appendTextValue(std::string &out, const CstNodeView &node,
                       const ValueBuildContext *context = nullptr) const override {
    if (!_hasCustomValueConverter) {
      appendDefaultTextValue(out, node, context);
      return;
    }
    appendConvertedValue(out, convertValue(node, context));
  }

private:
  friend struct detail::ParseAccess;
  using LocalRecoveryChoice = detail::TerminalRecoveryCandidate;
  using LocalRecoveryChoiceKind = detail::TerminalRecoveryChoiceKind;

  template <EditableParseModeContext Context>
  [[gnu::cold, gnu::noinline]]
  [[nodiscard]] const char *
  deleteScanMatchEnd(Context &ctx, const char *scanCursor) const noexcept {
    const char *matched = terminal(scanCursor);
    return detail::can_apply_recovery_match(ctx, matched) ? matched : nullptr;
  }

  template <EditableParseModeContext Context>
  [[gnu::cold, gnu::noinline]]
  [[nodiscard]] LocalRecoveryChoice
  selectLocalRecoveryChoice(Context &ctx, const char *cursorStart) const {
    const auto deleteCandidate = detail::evaluate_delete_scan_terminal_candidate(
        ctx, cursorStart,
        [this, &ctx](const char *scanCursor) noexcept {
          return deleteScanMatchEnd(ctx, scanCursor);
        },
        [this, &ctx](const char *matched) { ctx.leaf(matched, this); });
    if (deleteCandidate.kind != LocalRecoveryChoiceKind::None) {
      return deleteCandidate;
    }

    return detail::evaluate_insert_hidden_terminal_candidate(ctx, cursorStart,
                                                             this);
  }

  template <EditableParseModeContext Context>
  [[gnu::cold, gnu::noinline]]
  bool applyLocalRecoveryChoice(Context &ctx,
                                const LocalRecoveryChoice &choice) const {
    using enum LocalRecoveryChoiceKind;
    switch (choice.kind) {
    case InsertHidden:
      if (detail::apply_insert_hidden_recovery_edit(ctx, this)) {
        if constexpr (RecoveryParseModeContext<Context>) {
          PEGIUM_RECOVERY_TRACE("[terminal rule] insert-hidden ", getName(),
                                " offset=", ctx.cursorOffset());
        }
        return true;
      }
      return false;
    case DeleteScan:
      return detail::apply_delete_scan_terminal_candidate(
          ctx,
          [this, &ctx](const char *scanCursor) noexcept {
            return deleteScanMatchEnd(ctx, scanCursor);
          },
          [this, &ctx](const char *matched) {
            if constexpr (RecoveryParseModeContext<Context>) {
              PEGIUM_RECOVERY_TRACE("[terminal rule] delete-scan match ",
                                    getName(), " offset=", ctx.cursorOffset());
            }
            ctx.leaf(matched, this);
          });
    case WordBoundarySplit:
    case Fuzzy:
    case None:
      return false;
    }
    return false;
  }

  template <RecoveryParseModeContext Context>
  [[gnu::cold, gnu::noinline]]
  bool parseRecovery(Context &ctx, const char *matchedEnd) const {
    if (!ctx.isInRecoveryPhase()) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] enter ", getName(),
                            " offset=", ctx.cursorOffset());
      if (matchedEnd != nullptr) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                              " offset=", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      PEGIUM_RECOVERY_TRACE("[terminal rule] strict fail ", getName(),
                            " offset=", ctx.cursorOffset());
      return false;
    }
    if (matchedEnd != nullptr) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                            " offset=", ctx.cursorOffset());
      ctx.leaf(matchedEnd, this);
      return true;
    }
    if (!ctx.canEdit()) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] no-edit-window fail ",
                            getName(), " offset=", ctx.cursorOffset());
      return false;
    }
    const char *const cursorStart = ctx.cursor();
    const auto bestChoice = selectLocalRecoveryChoice(ctx, cursorStart);
    if (applyLocalRecoveryChoice(ctx, bestChoice)) {
      return true;
    }

    PEGIUM_RECOVERY_TRACE("[terminal rule] fail ", getName(),
                          " offset=", ctx.cursorOffset());
    return false;
  }

  template <ExpectParseModeContext Context>
  [[gnu::cold, gnu::noinline]]
  bool parseExpect(Context &ctx, const char *matchedEnd) const {
    if (ctx.reachedAnchor()) {
      ctx.addRule(this);
      return true;
    }
    if (matchedEnd != nullptr && ctx.canTraverseUntil(matchedEnd)) {
      ctx.leaf(matchedEnd, this);
      return true;
    }
    if (!ctx.canEdit()) {
      return false;
    }
    const char *const cursorStart = ctx.cursor();
    return applyLocalRecoveryChoice(ctx,
                                    selectLocalRecoveryChoice(ctx,
                                                              cursorStart));
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    const char *const matchedEnd = terminal(ctx.cursor());
    if constexpr (StrictParseModeContext<Context>) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] enter ", getName(),
                            " offset=", ctx.cursorOffset());
      if (matchedEnd != nullptr) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                              " offset=", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      PEGIUM_RECOVERY_TRACE("[terminal rule] strict fail ", getName(),
                            " offset=", ctx.cursorOffset());
      return false;
    } else if constexpr (RecoveryParseModeContext<Context>) {
      return parseRecovery(ctx, matchedEnd);
    } else {
      return parseExpect(ctx, matchedEnd);
    }
  }

public:

  template <NonNullableTerminalCapableExpression Element>
  TerminalRule &operator=(Element &&element) {
    BaseRule::operator=(std::forward<Element>(element));
    return *this;
  }
  void setValueConverter(
      std::function<opt::ConversionResult<T>(std::string_view)>
          &&value_converter) {
    _value_converter = std::move(value_converter);
    _hasCustomValueConverter = static_cast<bool>(_value_converter);
  }

private:
  std::function<opt::ConversionResult<T>(std::string_view)> _value_converter;
  bool _hasCustomValueConverter = false;

  [[nodiscard]] std::string genericConversionErrorMessage() const {
    return "Converter failed for rule " + std::string(getName());
  }

  [[nodiscard]] std::string missingConverterMessage() const {
    return "ValueConvert not provided for rule " + std::string(getName());
  }

  static constexpr std::string_view defaultConversionFailureMessage() noexcept {
    return "Conversion failed";
  }

  void reportConversionFailure(const CstNodeView &node,
                               const ValueBuildContext *context,
                               std::string_view message) const {
    if (context == nullptr) {
      return;
    }
    context->addConversionDiagnostic(node, this, std::string(message));
  }

  template <typename V>
  static void appendConvertedValue(std::string &out, V &&value) {
    using RawV = std::remove_cvref_t<V>;

    if constexpr (std::same_as<RawV, std::string_view>) {
      out.append(value);
    } else if constexpr (std::same_as<RawV, std::string>) {
      out.append(value);
    } else if constexpr (std::same_as<RawV, bool>) {
      out.append(value ? "true" : "false");
    } else if constexpr (std::same_as<RawV, std::nullptr_t>) {
      out.append("null");
    } else if constexpr (std::same_as<RawV, char>) {
      out += std::to_string(static_cast<int>(value));
    } else if constexpr (std::is_enum_v<RawV>) {
      appendConvertedValue(out, static_cast<std::underlying_type_t<RawV>>(value));
    } else if constexpr (std::integral<RawV> && !std::same_as<RawV, bool> &&
                         !std::same_as<RawV, char>) {
      std::array<char, 32> buffer{};
      const auto [ptr, ec] =
          std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
      if (ec == std::errc()) {
        out.append(buffer.data(), ptr);
      }
    } else if constexpr (std::floating_point<RawV>) {
      out += std::to_string(value);
    }
  }

  T convertDefaultValue(const CstNodeView &node,
                        const ValueBuildContext *context) const {
    const auto text = node.getText();
    if constexpr (std::is_same_v<T, std::string_view>) {
      return text;
    } else if constexpr (std::is_same_v<T, std::string>) {
      return std::string(text);
    } else if constexpr (std::is_same_v<T, bool>) {
      return text == "true";
    } else if constexpr ((std::is_integral_v<T> || std::is_floating_point_v<T>) &&
                         !std::is_same_v<T, bool> && !std::is_same_v<T, char>) {
      T value{};
      const auto [ptr, ec] =
          std::from_chars(text.data(), text.data() + text.size(), value);
      if (ec == std::errc() && ptr == text.data() + text.size()) {
        return value;
      }
      reportConversionFailure(node, context, defaultConversionFailureMessage());
      return {};
    } else {
      reportConversionFailure(node, context, missingConverterMessage());
      return {};
    }
  }

  void appendDefaultTextValue(std::string &out, const CstNodeView &node,
                              const ValueBuildContext *context) const {
    if constexpr (std::same_as<T, std::string_view> ||
                  std::same_as<T, std::string>) {
      const auto text = node.getText();
      out.append(text.data(), text.size());
    } else {
      appendConvertedValue(out, convertDefaultValue(node, context));
    }
  }

  T convertValue(const CstNodeView &node,
                 const ValueBuildContext *context) const {
    if (!_hasCustomValueConverter) {
      return convertDefaultValue(node, context);
    }
    const auto text = node.getText();
    auto result = _value_converter(text);
    if (result.has_value()) {
      return std::move(result).value();
    }
    if (result.error().empty()) {
      if (context != nullptr) {
        context->addConversionDiagnostic(node, this,
                                         genericConversionErrorMessage());
      }
    } else {
      reportConversionFailure(node, context, result.error());
    }
    return {};
  }

  template <typename Option> void applyOption(Option &&option) {
    using OptionType = std::remove_cvref_t<Option>;
    if constexpr (opt::IsConverterOption_v<OptionType>) {
      setValueConverterFromOption(std::forward<Option>(option).converter);
    } else {
      static_assert(opt::detail::DependentFalse_v<OptionType>,
                    "Unsupported option for TerminalRule. "
                    "Supported option: opt::with_converter(...).");
    }
  }

  template <typename Converter>
  void setValueConverterFromOption(Converter &&converter) {
    using ConverterType = std::remove_cvref_t<Converter>;
    static_assert(std::is_nothrow_invocable_v<ConverterType &, std::string_view>,
                  "TerminalRule converter must be noexcept and invocable with "
                  "std::string_view.");
    using ReturnType = std::invoke_result_t<ConverterType &, std::string_view>;
    static_assert(
        opt::IsConversionResultFor_v<ReturnType, T>,
        "TerminalRule converter must return "
        "opt::ConversionResult<T> (or a compatible value type).");

    _value_converter =
        [converterFn = std::forward<Converter>(converter)](
            std::string_view sv) mutable -> opt::ConversionResult<T> {
      auto result = std::invoke(converterFn, sv);
      if (result.has_value()) {
        return opt::conversion_value<T>(
            static_cast<T>(std::move(result).value()));
      }
      return opt::conversion_error<T>(result.error());
    };
    _hasCustomValueConverter = true;
  }
};

namespace detail {

template <typename T> struct IsTerminalRule : std::false_type {};

template <typename T>
struct IsTerminalRule<TerminalRule<T>> : std::true_type {};

template <typename T>
inline constexpr bool IsTerminalRule_v =
    IsTerminalRule<std::remove_cvref_t<T>>::value;

} // namespace detail

} // namespace pegium::parser
