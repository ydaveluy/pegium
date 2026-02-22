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
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/RuleValue.hpp>
#include <stdexcept>
#include <string>
#include <string_view>
namespace pegium::parser {

template <typename T = std::string>
  requires(!std::derived_from<T, AstNode>) &&
           detail::SupportedRuleValueType<T>
struct DataTypeRule final : AbstractRuleBase<grammar::DataTypeRule> {
  using type = T;
  using value_variant = grammar::RuleValue;
  using BaseRule = AbstractRuleBase<grammar::DataTypeRule>;
  using BaseRule::BaseRule;
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  value_variant getValue(const CstNodeView &node) const override {
    return detail::toRuleValue(_value_converter(node));
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

#if defined(PEGIUM_BENCH_RECOVERY_ONLY)
    constexpr bool bypassInitialStrictParse = true;
#else
    constexpr bool bypassInitialStrictParse = false;
#endif

    std::size_t maxCursorOffset = 0;
    if constexpr (!bypassInitialStrictParse) {
      ParseState state{builder, context};
      state.skipHiddenNodes();
      const bool match = parse_rule(state);
      result.len = static_cast<size_t>(state.cursor() - state.begin);
      maxCursorOffset = state.maxCursorOffset();
      result.ret = match && result.len == input.size();
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
      if (!node.has_value()) {
        throw std::logic_error("DataTypeRule::parse matched node not found");
      }
      result.value = _value_converter(*node);
    }

    return result;
  }
  bool parse_rule(ParseState &s) const {
    const auto mark = s.enter();
    if (!parse_assigned_rule(s)) {
      s.rewind(mark);
      return false;
    }
    s.exit(this);
    return true;
  }
  bool recover(RecoverState &recoverState) const {
    const auto mark = recoverState.enter();
    if (!parse_assigned_recover(recoverState)) {
      recoverState.rewind(mark);
      return false;
    }
    recoverState.exit(this);
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
        for (const auto &it : node) {
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
            value += static_cast<const grammar::CharacterRange *>(grammarElement)
                         ->getValue(it);
            break;
          }
          case ElementKind::AnyCharacter: {
            value += static_cast<const grammar::AnyCharacter *>(grammarElement)
                         ->getValue(it);
            break;
          }
          default:
            value += it.getText();
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
