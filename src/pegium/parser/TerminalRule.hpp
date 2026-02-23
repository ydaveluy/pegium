#pragma once

#include <algorithm>
#include <cassert>
#include <charconv>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/parser/Skipper.hpp>
#include <stdexcept>
#include <string>

namespace pegium::parser {

template <typename T = std::string>
  requires detail::SupportedRuleValueType<T>
struct TerminalRule final : AbstractRule<grammar::TerminalRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRule<grammar::TerminalRule>;
  using BaseRule::BaseRule;
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }
  inline T getTypedValue(const CstNodeView &node) const {
    if (!node.isRecovered()) [[likely]] {
      return _value_converter(node.getText());
    }
    // If the node is recovered, the text may not be valid for conversion, so we
    // catch any exceptions and return a default value.
    try {
      return _value_converter(node.getText());
    } catch (...) {
      // ignore the conversion error and return a default value for the type.
      return {};
    }
  }

  value_variant getValue(const CstNodeView &node) const override {
    return detail::toRuleValue(getTypedValue(node));
  }
  GenericParseResult parseGeneric(std::string_view text, const Skipper &skipper,
                                  const ParseOptions &options = {}) const {
    auto result = parse(text, skipper, options);
    return {.root_node = result.root_node};
  }
  ParseResult<T> parse(std::string_view text, const Skipper &skipper,
                       const ParseOptions &options = {}) const {
    ParseResult<T> result;
    CstBuilder builder(text);
    const auto input = builder.getText();

    std::uint32_t maxCursorOffset = 0;
    auto runRecoveryAttempt = [&](bool strictNoEdit, bool resetBuilder,
                                  std::uint32_t editFloorOffset,
                                  std::uint32_t editCeilingOffset) {
      if (resetBuilder)
        builder.reset();
      ParseContext ctx{builder, skipper};
      ctx.setEditFloorOffset(editFloorOffset);
      ctx.setEditCeilingOffset(editCeilingOffset);
      ctx.setTrackEditState(!strictNoEdit);
      ctx.setMaxConsecutiveCodepointDeletes(
          options.maxConsecutiveCodepointDeletes);
      if (strictNoEdit) {
        ctx.allowInsert = false;
        ctx.allowDelete = false;
      }
      ctx.skipHiddenNodes();
      const bool recoveredMatch = rule(ctx);
      result.len = static_cast<size_t>(ctx.cursor() - ctx.begin);
      result.recovered = ctx.hadEdits;
      result.diagnostics = ctx.diagnostics;
      result.ret = recoveredMatch && result.len == input.size();
      return ctx.maxCursorOffset();
    };

    const std::uint32_t strictMax =
        runRecoveryAttempt(/*strictNoEdit=*/true,
                           /*resetBuilder=*/false,
                           /*editFloorOffset=*/0,
                           /*editCeilingOffset=*/input.size());
    maxCursorOffset = std::max(maxCursorOffset, strictMax);

    if (!result.ret && options.recoveryEnabled) {
      const std::uint32_t localRecoveryWindow =
          options.localRecoveryWindowBytes;
      if (localRecoveryWindow != 0) {
        const std::uint32_t recoveryFloorAnchor = maxCursorOffset;
        const std::uint32_t localEditCeilingOffset =
            std::min(static_cast<std::uint32_t>(input.size()),
                     recoveryFloorAnchor + localRecoveryWindow);
        runRecoveryAttempt(/*strictNoEdit=*/false, /*resetBuilder=*/true,
                           /*editFloorOffset=*/recoveryFloorAnchor,
                           localEditCeilingOffset);
        if (!result.ret) {
          runRecoveryAttempt(/*strictNoEdit=*/false, /*resetBuilder=*/true,
                             /*editFloorOffset=*/recoveryFloorAnchor,
                             /*editCeilingOffset=*/input.size());
        }
      } else {
        while (!result.ret) {
          const std::size_t newMax = runRecoveryAttempt(
              /*strictNoEdit=*/false, /*resetBuilder=*/true,
              /*editFloorOffset=*/maxCursorOffset,
              /*editCeilingOffset=*/input.size());
          if (newMax <= maxCursorOffset)
            break;
          maxCursorOffset = newMax;
        }
      }
    }

    result.root_node = builder.finalize();
    if (result.ret) {
      auto node = detail::findFirstRootMatchingNode(*result.root_node, this);
      if (!node.has_value())
        node = detail::findFirstMatchingNode(*result.root_node, this);
      if (node.has_value())
        result.value = _value_converter(node->getText());
    }

    return result;
  }

  bool rule(ParseContext &ctx) const {
    PEGIUM_RECOVERY_TRACE("[terminal rule] enter ", getName(),
                          " offset=", ctx.cursorOffset());
    auto i = terminal_fast(ctx.cursor(), ctx.end);
    if (i.IsValid()) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] direct match ", getName(),
                            " offset=", ctx.cursorOffset());
      ctx.leaf(i.offset, this);
      ctx.skipHiddenNodes();
      return true;
    }

    if (ctx.isStrictNoEditMode()) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] strict fail ", getName(),
                            " offset=", ctx.cursorOffset());
      return false;
    }

    const auto mark = ctx.mark();
    if (ctx.insertHidden(this)) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] inserted ", getName(),
                            " offset=", ctx.cursorOffset());
      ctx.skipHiddenNodes();
      return true;
    }
    if (ctx.insertHiddenForced(this)) {
      PEGIUM_RECOVERY_TRACE("[terminal rule] forced insert ", getName(),
                            " offset=", ctx.cursorOffset());
      ctx.skipHiddenNodes();
      return true;
    }
    while (ctx.deleteOneCodepoint()) {
      i = terminal(ctx.cursor(), ctx.end);
      if (i.IsValid()) {
        PEGIUM_RECOVERY_TRACE("[terminal rule] delete-scan match ", getName(),
                              " offset=", ctx.cursorOffset());
        ctx.leaf(i.offset, this);
        ctx.skipHiddenNodes();
        return true;
      }
    }

    PEGIUM_RECOVERY_TRACE("[terminal rule] fail ", getName(),
                          " offset=", ctx.cursorOffset());
    ctx.rewind(mark);
    return false;
  }

  using BaseRule::operator=;
  void setValueConverter(std::function<T(std::string_view)> &&value_converter) {
    _value_converter = std::move(value_converter);
  }

private:
  std::function<T(std::string_view)> _value_converter =
      initializeDefaultValueConverter();

  std::function<T(std::string_view)> initializeDefaultValueConverter() {
    // initialize the value converter for standard types
    if constexpr (std::is_same_v<T, std::string_view>) {
      return [](std::string_view sv) { return sv; };
    } else if constexpr (std::is_same_v<T, std::string>) {
      return [](std::string_view sv) { return std::string(sv); };
    } else if constexpr (std::is_same_v<T, bool>) {
      return [](std::string_view sv) { return sv == "true"; };
    } else if constexpr (std::is_integral_v<T> || std::is_floating_point_v<T>) {
      return [](std::string_view sv) {
        T value;
#ifndef NDEBUG
        auto [ptr, ec] =
            std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc() || ptr != sv.data() + sv.size()) {
          throw std::invalid_argument("Conversion failed");
        }
#else
        // skip the error checking for performance in release mode.
        std::from_chars(sv.data(), sv.data() + sv.size(), value);
#endif
        return value;
      };
    }
    return [this](std::string_view) -> T {
      throw std::logic_error("ValueConvert not provided for rule " + getName());
    };
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
