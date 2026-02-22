#pragma once

#include <charconv>
#include <algorithm>
#include <cassert>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <stdexcept>
#include <string>

namespace pegium::parser {

template <typename T = std::string>
  requires detail::SupportedRuleValueType<T>
struct TerminalRule final : AbstractRuleBase<grammar::TerminalRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRuleBase<grammar::TerminalRule>;
  using BaseRule::BaseRule;
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  value_variant getValue(const CstNodeView &node) const override {
    return detail::toRuleValue(_value_converter(node.getText()));
  }
  GenericParseResult parseGeneric(std::string_view text,
                                  const ParseContext &context,
                                  const ParseOptions &options = {}) const {
    auto result = parse(text, context, options);
    return {.root_node = result.root_node};
  }
  ParseResult<T> parse(std::string_view text, const ParseContext &context,
                       const ParseOptions &options = {}) const {
    ParseResult<T> result;
    CstBuilder builder(text);
    const auto input = builder.getText();
    const char *const start = input.data();
    const char *const end = start + input.size();

#if defined(PEGIUM_BENCH_RECOVERY_ONLY)
    constexpr bool bypassInitialStrictParse = true;
#else
    constexpr bool bypassInitialStrictParse = false;
#endif

    std::size_t maxCursorOffset = 0;
    if constexpr (!bypassInitialStrictParse) {
      const char *const begin = start;
      auto match = parse_terminal(begin, end);
      result.len = static_cast<size_t>(match.offset - start);
      maxCursorOffset = result.len;
      result.ret = match.IsValid() && result.len == input.size();
      if (match.IsValid()) {
        builder.leaf(begin, match.offset, this);
      }

      if (result.ret) {
        result.root_node = builder.finalize();
        result.value = _value_converter(std::string_view{
            begin, static_cast<size_t>(match.offset - begin)});
        return result;
      }
    } else {
      result.len = 0;
      result.ret = false;
      maxCursorOffset = 0;
    }

    if (!result.ret) {
      auto runRecoveryAttempt = [&](bool strictNoEdit, bool resetBuilder,
                                    std::size_t editFloorOffset,
                                    std::size_t editCeilingOffset)
          -> std::size_t {
        if (resetBuilder) {
          builder.reset();
        }
        RecoverState recoverState{builder, context};
        recoverState.setEditFloorOffset(editFloorOffset);
        recoverState.setEditCeilingOffset(editCeilingOffset);
        recoverState.setTrackEditState(!strictNoEdit);
        recoverState.setMaxConsecutiveCodepointDeletes(
            options.maxConsecutiveCodepointDeletes);
        if (strictNoEdit) {
          recoverState.allowInsert = false;
          recoverState.allowDelete = false;
        }
        recoverState.skipHiddenNodes();
        const bool recoveredMatch = recover(recoverState);
        result.len =
            static_cast<size_t>(recoverState.cursor() - recoverState.begin);
        result.recovered = recoverState.hadEdits;
        result.diagnostics = recoverState.diagnostics;
        result.ret = recoveredMatch && result.len == input.size();
        return recoverState.maxCursorOffset();
      };

      if constexpr (bypassInitialStrictParse) {
        const std::size_t strictMax =
            runRecoveryAttempt(/*strictNoEdit=*/true, /*resetBuilder=*/false,
                               /*editFloorOffset=*/0,
                               /*editCeilingOffset=*/input.size());
        if (!result.ret && strictMax > maxCursorOffset) {
          maxCursorOffset = strictMax;
        }
      }

      if (!result.ret) {
        const std::size_t localRecoveryWindow = options.localRecoveryWindowBytes;
        if (localRecoveryWindow != 0) {
          const std::size_t recoveryFloorAnchor = maxCursorOffset;
          const std::size_t localEditCeilingOffset = std::min(
              input.size(), recoveryFloorAnchor + localRecoveryWindow);
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
            const std::size_t newMax =
                runRecoveryAttempt(/*strictNoEdit=*/false, /*resetBuilder=*/true,
                                   /*editFloorOffset=*/maxCursorOffset,
                                   /*editCeilingOffset=*/input.size());
            if (newMax <= maxCursorOffset) {
              break;
            }
            maxCursorOffset = newMax;
          }
        }
      }
    }
    result.root_node = builder.finalize();
    if (result.ret) {
      auto node = detail::findFirstRootMatchingNode(*result.root_node, this);
      if (!node.has_value()) {
        node = detail::findFirstMatchingNode(*result.root_node, this);
      }
      if (node.has_value()) {
        result.value = _value_converter(node->getText());
      }
    }

    return result;
  }

  bool parse_rule(ParseState &s) const {

    assert(element && "The rule definition is missing !");
    auto i = parse_assigned_terminal_fast(s.cursor(), s.end);
    if (i.IsValid()) {
      s.leaf(i.offset, this);
      // skip hidden nodes after the token
      s.skipHiddenNodes();
      return true;
    }

    return false;
  }
  bool recover(RecoverState &recoverState) const {
    PEGIUM_RECOVERY_TRACE("[terminal recover] enter ", getName(), " offset=",
                          recoverState.cursorOffset());
    auto i = parse_terminal(recoverState.cursor(), recoverState.end);
    if (i.IsValid()) {
      PEGIUM_RECOVERY_TRACE("[terminal recover] direct match ", getName(),
                            " offset=", recoverState.cursorOffset());
      recoverState.leaf(i.offset, this);
      recoverState.skipHiddenNodes();
      return true;
    }

    if (recoverState.isStrictNoEditMode()) {
      PEGIUM_RECOVERY_TRACE("[terminal recover] strict fail ", getName(),
                            " offset=", recoverState.cursorOffset());
      return false;
    }

    const auto mark = recoverState.mark();
    if (recoverState.insertHidden(this)) {
      PEGIUM_RECOVERY_TRACE("[terminal recover] inserted ", getName(),
                            " offset=", recoverState.cursorOffset());
      recoverState.skipHiddenNodes();
      return true;
    }
    if (recoverState.insertHiddenForced(this)) {
      PEGIUM_RECOVERY_TRACE("[terminal recover] forced insert ", getName(),
                            " offset=", recoverState.cursorOffset());
      recoverState.skipHiddenNodes();
      return true;
    }
    while (recoverState.deleteOneCodepoint()) {
      i = parse_terminal(recoverState.cursor(), recoverState.end);
      if (i.IsValid()) {
        PEGIUM_RECOVERY_TRACE("[terminal recover] delete-scan match ",
                              getName(), " offset=", recoverState.cursorOffset());
        recoverState.leaf(i.offset, this);
        recoverState.skipHiddenNodes();
        return true;
      }
    }

    PEGIUM_RECOVERY_TRACE("[terminal recover] fail ", getName(), " offset=",
                          recoverState.cursorOffset());
    recoverState.rewind(mark);
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
    } else if constexpr (std::is_integral_v<T>) {
      return [](std::string_view sv) {
        T value;
        auto [ptr, ec] =
            std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc()) {
          throw std::invalid_argument("Conversion failed");
        }
        return value;
      };
    } else if constexpr (std::is_floating_point_v<T>) {
      return [](std::string_view sv) {
        T value;
        auto [ptr, ec] =
            std::from_chars(sv.data(), sv.data() + sv.size(), value);
        if (ec != std::errc() || ptr != sv.data() + sv.size()) {
          throw std::invalid_argument("Conversion failed");
        }
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
