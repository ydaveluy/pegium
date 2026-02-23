#pragma once
#include <algorithm>
#include <cassert>
#include <functional>
#include <pegium/grammar/AnyCharacter.hpp>
#include <pegium/grammar/CharacterRange.hpp>
#include <pegium/grammar/Literal.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <pegium/parser/Skipper.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
namespace pegium::parser {

template <typename T = std::string>
  requires(!std::derived_from<T, AstNode>) && detail::SupportedRuleValueType<T>
struct DataTypeRule final : AbstractRule<grammar::DataTypeRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRule<grammar::DataTypeRule>;
  using BaseRule::BaseRule;
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }
  T getTypedValue(const CstNodeView &node) const {
    if (!node.isRecovered()) [[likely]] {
      return _value_converter(node);
    }
    // If the node is recovered, the text may not be valid for conversion, so we
    // catch any exceptions and return a default value.
    try {
      return _value_converter(node);
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
      ParseContext context{builder, skipper};
      context.setEditFloorOffset(editFloorOffset);
      context.setEditCeilingOffset(editCeilingOffset);
      context.setTrackEditState(!strictNoEdit);
      context.setMaxConsecutiveCodepointDeletes(
          options.maxConsecutiveCodepointDeletes);
      if (strictNoEdit) {
        context.allowInsert = false;
        context.allowDelete = false;
      }
      context.skipHiddenNodes();
      const bool recoveredMatch = rule(context);

      result.len = static_cast<size_t>(context.cursor() - context.begin);
      result.recovered = context.hadEdits;
      result.diagnostics = context.diagnostics;
      result.ret = recoveredMatch && result.len == input.size();
      return context.maxCursorOffset();
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
          const std::uint32_t newMax = runRecoveryAttempt(
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
      if (!node.has_value())
        throw std::logic_error("DataTypeRule::parse matched node not found");
      result.value = _value_converter(*node);
    }

    return result;
  }

  bool rule(ParseContext &ctx) const {
    const auto mark = ctx.enter();
    if (!rule_fast(ctx)) {
      ctx.rewind(mark);
      return false;
    }
    ctx.exit(mark, this);
    return true;
  }

  using BaseRule::operator=;

private:
  std::function<T(const CstNodeView &)> _value_converter =
      initializeDefaultValueConverter();

  std::function<T(const CstNodeView &)> initializeDefaultValueConverter() {
    if constexpr (std::is_same_v<T, std::string>) {
      return [](const CstNodeView &node) {
        std::string value;
        for (const auto it : node) {
          const auto *grammarElement = it.getGrammarElement();
          assert(grammarElement);

          if (it.isHidden()) {
            continue;
          }

          switch (grammarElement->getKind()) {
          case ElementKind::Literal: {
            value += static_cast<const grammar::Literal *>(grammarElement)
                         ->getValue(it);
            break;
          }
          case ElementKind::CharacterRange: {
            value +=
                static_cast<const grammar::CharacterRange *>(grammarElement)
                    ->getValue(it);
            break;
          }
          case ElementKind::AnyCharacter: {
            value += static_cast<const grammar::AnyCharacter *>(grammarElement)
                         ->getValue(it);
            break;
          }

          case ElementKind::TerminalRule: {
            auto terminalValue = static_cast<const grammar::TerminalRule *>(grammarElement)
                         ->getValue(it);

            std::visit(
                [&value](auto &&arg) {
                  using V = std::decay_t<decltype(arg)>;
                  if constexpr (std::is_same_v<V, std::string_view> || std::is_same_v<V, std::string>) {
                    value += arg;
                  } else if constexpr (std::is_arithmetic_v<V> || std::is_floating_point_v<V>) {
                    value += std::to_string(arg);
                  } else if constexpr (std::is_same_v<V, bool>) {
                    value += arg ? "true" : "false";
                  } else if constexpr (std::is_same_v<V, std::nullptr_t>) {
                    value += "null";
                  } else {
                
                  }
                },
                terminalValue);
            break;
          }
          default:
            value += it.getText();
            throw std::logic_error(std::string("ValueConvert not provided for rule ") + std::to_string(static_cast<int>(grammarElement->getKind())));
            break;
          }
        }
        return value;
      };
    }
    return [this](const CstNodeView &) -> T {
      throw std::logic_error("ValueConvert not provided for rule " + getName());
    };
  }
};

} // namespace pegium::parser
