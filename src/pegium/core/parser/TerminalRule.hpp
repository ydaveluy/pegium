#pragma once

/// Parser rule template producing terminal scalar values.

#include <array>
#include <cassert>
#include <charconv>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <pegium/core/parser/AbstractRule.hpp>
#include <pegium/core/parser/ExpectContext.hpp>
#include <pegium/core/parser/Introspection.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseMode.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryTrace.hpp>
#include <pegium/core/parser/RuleOptions.hpp>
#include <pegium/core/parser/RuleValue.hpp>
#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/ValueBuildContext.hpp>
#include <string>

namespace pegium::parser {

template <Expression... Elements> struct Group;

template <typename T = std::string>
  requires detail::SupportedRuleValueType<T>
struct TerminalRule final : AbstractRule<grammar::TerminalRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRule<grammar::TerminalRule>;
  static constexpr bool isFailureSafe = true;

  template <NonNullableTerminalCapableExpression Element>
  constexpr TerminalRule(std::string_view name, Element &&element)
      : TerminalRule(name,
                     detail::infer_direct_literal_recovery_metadata(element),
                     std::forward<Element>(element)) {}

  template <NonNullableTerminalCapableExpression Element, typename... Options>
    requires(sizeof...(Options) > 0)
  constexpr TerminalRule(std::string_view name, Element &&element,
                         Options &&...options)
      : TerminalRule(name,
                     detail::infer_direct_literal_recovery_metadata(element),
                     std::forward<Element>(element)) {
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

  bool probeRecoverable(RecoveryContext &ctx) const noexcept {
    if (probeRecoverableAtEntry(ctx)) {
      return true;
    }
    return fuzzy_candidate_match(
               ctx, ctx.cursor(),
               [](const detail::LiteralFuzzyCandidate &) noexcept {
                 return true;
               }) ||
           delete_scan_match(ctx);
  }

  bool probeRecoverableAtEntry(RecoveryContext &ctx) const noexcept {
    return fuzzy_candidate_match(ctx, ctx.cursor(),
                                 detail::allows_entry_probe_fuzzy_candidate) ||
           detail::allows_terminal_entry_probe(ctx, _terminalShape) ||
           delete_scan_match(ctx);
  }

  bool
  probeRecoverableAtEntryConsumesVisible(RecoveryContext &ctx) const noexcept {
    const char *const cursorStart = ctx.cursor();
    if (const char *const matchedEnd = terminal(cursorStart);
        matchedEnd != nullptr && matchedEnd > cursorStart) {
      return true;
    }
    return fuzzy_candidate_match(
               ctx, cursorStart,
               [](const detail::LiteralFuzzyCandidate &candidate) noexcept {
                 return candidate.consumed > 0u &&
                        detail::allows_entry_probe_fuzzy_candidate(candidate);
               }) ||
           delete_scan_match(ctx);
  }

private:
  /// Iterates literal-replace fuzzy candidates near the cursor and returns
  /// true as soon as `predicate(candidate)` accepts one. Returns false when no
  /// metadata is available or no candidate matches.
  template <typename Predicate>
  bool fuzzy_candidate_match(RecoveryContext &ctx, const char *cursorStart,
                             Predicate &&predicate) const noexcept {
    if (!_literalRecoveryMetadata.has_value() ||
        (ctx.hasHadEdits() &&
         !detail::allows_fuzzy_replace_after_prior_edits(_terminalShape))) {
      return false;
    }
    const auto replaceCandidates = detail::collect_literal_replace_candidates(
        ctx, cursorStart, _literalRecoveryMetadata->value,
        _literalRecoveryMetadata->caseSensitive, _terminalShape);
    return std::ranges::any_of(replaceCandidates,
                               std::forward<Predicate>(predicate));
  }

  bool delete_scan_match(RecoveryContext &ctx) const noexcept {
    const auto localRecoveryFacts = detail::TerminalRecoveryFacts{
        .triviaGap = detail::current_local_skip_trivia_gap_profile(ctx),
    };
    return detail::probe_nearby_delete_scan_match(
        ctx,
        [this](const char *scanCursor) noexcept { return terminal(scanCursor); },
        localRecoveryFacts, _terminalShape);
  }

public:

  TerminalRule super() const { return *this; }
  inline T getRawValue(const CstNodeView &node) const {
    return convertValue(node, nullptr);
  }

  inline T getRawValue(const CstNodeView &node,
                       const ValueBuildContext &context) const {
    return convertValue(node, &context);
  }

  value_variant
  getValue(const CstNodeView &node,
           const ValueBuildContext *context = nullptr) const override {
    return detail::toRuleValue(convertValue(node, context));
  }

  void
  appendTextValue(std::string &out, const CstNodeView &node,
                  const ValueBuildContext *context = nullptr) const override {
    if (!_hasCustomValueConverter) {
      appendDefaultTextValue(out, node, context);
      return;
    }
    appendConvertedValue(out, convertValue(node, context));
  }

private:
  friend struct detail::ParseAccess;
  template <Expression... Elements> friend struct Group;
  template <typename> friend struct SkipperWrapped;

  template <NonNullableTerminalCapableExpression Element>
  constexpr TerminalRule(std::string_view name,
                         std::optional<detail::DirectLiteralRecoveryMetadata>
                             literalRecoveryMetadata,
                         Element &&element)
      : BaseRule(name, std::forward<Element>(element)),
        // Derive the shape from the same metadata the matcher reads (one
        // source of truth); `_terminalShape` is declared before
        // `_literalRecoveryMetadata`, so this reads the param before the move.
        _terminalShape(
            detail::terminal_shape_from_recovery_metadata(literalRecoveryMetadata)),
        _literalRecoveryMetadata(std::move(literalRecoveryMetadata)) {
    static_assert(
        detail::ConsistentLiteralRecoveryElement<std::remove_cvref_t<Element>>,
        "A terminal element exposing getValue() must also expose "
        "isCaseSensitive() so the recovery shape and matcher metadata stay "
        "consistent.");
  }

  detail::TerminalShape _terminalShape{};
  std::optional<detail::DirectLiteralRecoveryMetadata>
      _literalRecoveryMetadata{};

  template <EditableParseModeContext Context>
  bool parse_terminal_recovery_impl(
      Context &ctx,
      const detail::TerminalRecoveryFacts &terminalRecoveryFacts) const {
    const auto effectiveRecoveryFacts =
        detail::effective_terminal_recovery_facts(ctx, terminalRecoveryFacts);
    const char *const cursorStart = ctx.cursor();
    const char *const matchedEnd = terminal(cursorStart);
    const auto applyRecoveredLeaf = [this, &ctx](const char *matched) {
      if constexpr (RecoveryParseModeContext<Context>) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] delete-scan match ", getName(),
                              " offset=", ctx.cursorOffset());
      }
      ctx.leaf(matched, this, false, true);
    };
    const auto matchRecoverableTerminal =
        [this, &ctx,
         cursorStart](const char *scanCursor) noexcept -> const char * {
      if (!detail::allows_extended_terminal_delete_scan_match(ctx, cursorStart,
                                                              scanCursor)) {
        return nullptr;
      }
      const char *const matched = terminal(scanCursor);
      return detail::can_apply_recovery_match(ctx, matched) ? matched : nullptr;
    };
    const auto try_local_recovery = [this, &ctx, cursorStart,
                                     &effectiveRecoveryFacts,
                                     &matchRecoverableTerminal,
                                     &applyRecoveredLeaf]() {
      const auto &facts = effectiveRecoveryFacts;
      const bool hasHadEdits =
          []<typename Ctx>(const Ctx &currentCtx) constexpr noexcept {
            if constexpr (requires { currentCtx.hasHadEdits(); }) {
              return currentCtx.hasHadEdits();
            }
            return false;
          }(ctx);
      detail::TerminalRecoveryCandidate bestChoice;
      if (_literalRecoveryMetadata.has_value()) {
        if (!hasHadEdits ||
            detail::allows_fuzzy_replace_after_prior_edits(
                _terminalShape)) {
          const auto replaceCandidates =
              detail::collect_literal_replace_candidates(
                  ctx, cursorStart, _literalRecoveryMetadata->value,
                  _literalRecoveryMetadata->caseSensitive,
                  _terminalShape, facts);
          for (const auto &replaceCandidate : replaceCandidates) {
            const char *const replaceEnd =
                cursorStart + replaceCandidate.consumed;
            if (!detail::can_apply_recovery_match(ctx, replaceEnd)) {
              continue;
            }
            const auto candidate =
                detail::evaluate_replace_leaf_terminal_candidate(
                    ctx, cursorStart, replaceEnd, this, replaceCandidate.cost,
                    replaceCandidate.distance);
            if (detail::is_better_recovery_key(
                    detail::terminal_recovery_key(candidate),
                    detail::terminal_recovery_key(bestChoice))) {
              bestChoice = candidate;
            }
          }
        }
      }
      const bool allowInsert =
          detail::allows_terminal_rule_insert(ctx, _terminalShape) &&
          (!_literalRecoveryMetadata.has_value() ||
           _terminalShape.allowsInsert());
      const auto choice = detail::complete_terminal_recovery_choice(
          ctx, cursorStart, this, facts,
          _terminalShape, allowInsert, bestChoice,
          matchRecoverableTerminal,
          [this, &ctx](const char *matched) {
            ctx.leaf(matched, this, false, true);
          });
      return detail::apply_terminal_recovery_choice(
          ctx, choice, this, []() { return false; },
          [this, &ctx, cursorStart, choice]() {
            if (const char *const replaceEnd = cursorStart + choice.consumed;
                !detail::can_apply_recovery_match(ctx, replaceEnd) ||
                !ctx.replaceLeaf(replaceEnd, this, choice.cost.budgetCost,
                                 /*hidden=*/false, choice.distance)) {
              return false;
            }
            if constexpr (RecoveryParseModeContext<Context>) {
              PEGIUM_RECOVERY_TRACE("[terminal rule] replace-literal ",
                                    getName(), " offset=", ctx.cursorOffset(),
                                    " distance=", choice.distance,
                                    " cost=", choice.cost.budgetCost,
                                    " rank=", choice.cost.primaryRankCost);
            }
            return true;
          },
          [this, &ctx]() {
            if constexpr (RecoveryParseModeContext<Context>) {
              PEGIUM_RECOVERY_TRACE("[terminal rule] insert-synthetic ",
                                    getName(), " offset=", ctx.cursorOffset());
            }
          },
          [this, &ctx, &facts, &matchRecoverableTerminal,
           &applyRecoveredLeaf]() {
            return detail::recover_by_terminal_delete_scan(
                ctx, matchRecoverableTerminal, applyRecoveredLeaf,
                facts, _terminalShape);
          });
    };

    if constexpr (RecoveryParseModeContext<Context>) {
      const bool notInPhase = !ctx.isInRecoveryPhase() &&
                              !ctx.hasPendingCommittedRecoveryEdits();
      if (notInPhase) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] enter ", getName(),
                              " offset=", ctx.cursorOffset());
      }
      if (matchedEnd != nullptr) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                              " offset=", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      // Outside an active recovery phase a strict miss stops here (no local
      // recovery is attempted until a window opens).
      if (notInPhase) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] strict fail ", getName(),
                              " offset=", ctx.cursorOffset());
        return false;
      }
      if (effectiveRecoveryFacts.localRecoveryBlocked) {
        return false;
      }
      if (!ctx.canEdit()) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] no-edit-window fail ", getName(),
                              " offset=", ctx.cursorOffset());
        return false;
      }
      if (try_local_recovery()) {
        return true;
      }

      PEGIUM_RECOVERY_TRACE("[terminal rule] fail ", getName(),
                            " offset=", ctx.cursorOffset());
      return false;
    } else {
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
      return try_local_recovery();
    }
  }

  template <ParseModeContext Context> bool parse_impl(Context &ctx) const {
    if constexpr (StrictParseModeContext<Context>) {
      const char *const cursorStart = ctx.cursor();
      PEGIUM_RECOVERY_TRACE("[terminal rule] enter ", getName(),
                            " offset=", ctx.cursorOffset());
      if (const char *const matchedEnd = terminal(cursorStart);
          matchedEnd != nullptr) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                              " offset=", ctx.cursorOffset());
        ctx.leaf(matchedEnd, this);
        return true;
      }
      PEGIUM_RECOVERY_TRACE("[terminal rule] strict fail ", getName(),
                            " offset=", ctx.cursorOffset());
      return false;
    } else {
      return parse_terminal_recovery_impl(ctx, {});
    }
  }

public:
  template <NonNullableTerminalCapableExpression Element>
  TerminalRule &operator=(Element &&element) {
    static_assert(
        detail::ConsistentLiteralRecoveryElement<std::remove_cvref_t<Element>>,
        "A terminal element exposing getValue() must also expose "
        "isCaseSensitive() so the recovery shape and matcher metadata stay "
        "consistent.");
    _literalRecoveryMetadata =
        detail::infer_direct_literal_recovery_metadata(element);
    _terminalShape =
        detail::terminal_shape_from_recovery_metadata(_literalRecoveryMetadata);
    BaseRule::operator=(std::forward<Element>(element));
    return *this;
  }
  void
  setValueConverter(std::function<opt::ConversionResult<T>(std::string_view)>
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
      appendConvertedValue(out,
                           static_cast<std::underlying_type_t<RawV>>(value));
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

  [[nodiscard]] static constexpr bool
  is_zero_width_recovered_leaf(const CstNodeView &node) noexcept {
    return node.isRecovered() && node.getBegin() == node.getEnd();
  }

  [[nodiscard]] std::string_view
  text_value(const CstNodeView &node) const noexcept {
    if (node.isRecovered() && _literalRecoveryMetadata.has_value()) {
      return _literalRecoveryMetadata->value;
    }
    return node.getText();
  }

  T convertDefaultValue(const CstNodeView &node,
                        const ValueBuildContext *context) const {
    if (is_zero_width_recovered_leaf(node)) {
      return {};
    }
    const auto text = text_value(node);
    if constexpr (std::is_same_v<T, std::string_view>) {
      return text;
    } else if constexpr (std::is_same_v<T, std::string>) {
      return std::string(text);
    } else if constexpr (std::is_same_v<T, bool>) {
      return text == "true";
    } else if constexpr ((std::is_integral_v<T> ||
                          std::is_floating_point_v<T>) &&
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
      const auto text = text_value(node);
      out.append(text.data(), text.size());
    } else {
      appendConvertedValue(out, convertDefaultValue(node, context));
    }
  }

  T convertValue(const CstNodeView &node,
                 const ValueBuildContext *context) const {
    if (is_zero_width_recovered_leaf(node)) {
      return {};
    }
    if (!_hasCustomValueConverter) {
      return convertDefaultValue(node, context);
    }
    const auto text = text_value(node);
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
    static_assert(
        std::is_nothrow_invocable_v<ConverterType &, std::string_view>,
        "TerminalRule converter must be noexcept and invocable with "
        "std::string_view.");

    // The converter may return opt::ConversionResult<T> (fallible) or a value
    // convertible to T directly (infallible); as_conversion_result normalizes
    // both and statically rejects anything else.
    _value_converter =
        [converterFn = std::forward<Converter>(converter)](
            std::string_view sv) mutable -> opt::ConversionResult<T> {
      return opt::as_conversion_result<T>(std::invoke(converterFn, sv));
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
