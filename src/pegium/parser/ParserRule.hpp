#pragma once
#include <pegium/grammar/Action.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/Action.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/ParseState.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/RecoverState.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <cassert>
#include <iostream>
#include <algorithm>
#include <stdexcept>
#include <string_view>

namespace pegium::parser {

template <typename T>
  requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRuleBase<grammar::ParserRule> {
  using type = T;
  using BaseRule = AbstractRuleBase<grammar::ParserRule>;
  using BaseRule::BaseRule;
  std::string_view getTypeName() const noexcept override {
    static constexpr auto typeName = detail::type_name_v<T>;
    return typeName;
  }

  std::shared_ptr<AstNode> getValue(const CstNodeView &node) const override {
    return std::static_pointer_cast<AstNode>(getTypedValue(node));
  }

  std::shared_ptr<T> getTypedValue(const CstNodeView &node) const {
    std::shared_ptr<T> current;
    struct AssignmentEntry {
      const grammar::Assignment *assignment;
      CstNodeView node;
    };
    std::vector<AssignmentEntry> assignments{};

    for (const auto &it : node) {
      if (it.isHidden()) {
        continue;
      }
      assert(it.getGrammarElement());
      switch (it.getGrammarElement()->getKind()) {
      case ElementKind::Assignment: {
        assignments.emplace_back(
            static_cast<const grammar::Assignment *>(it.getGrammarElement()), it);
        break;
      }
      case ElementKind::New: {
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::Action *>(it.getGrammarElement())
                ->execute(std::static_pointer_cast<AstNode>(current)));
        break;
      }
      case ElementKind::Init: {
        if (!current) {
          current = std::make_shared<T>();
        }
        for (const auto &entry : assignments) {
          entry.assignment->execute(current.get(), entry.node);
        }
        assignments.clear();
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::Action *>(it.getGrammarElement())
                ->execute(std::static_pointer_cast<AstNode>(current)));
        break;
      }
      case ElementKind::ParserRule: {
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::ParserRule *>(it.getGrammarElement())
                ->getValue(it));
        break;
      }
      default:
        break;
      }
    }

    if (!current) {
      current = std::make_shared<T>();
    }
    for (const auto &entry : assignments) {
      entry.assignment->execute(current.get(), entry.node);
    }
    return current;
  }
  GenericParseResult parseGeneric(std::string_view text,
                                  const ParseContext &context,
                                  const ParseOptions &options = {}) const {
    auto result = parse(text, context, options);
    return {.root_node = result.root_node};
  }
  ParseResult<std::shared_ptr<T>>
  parse(std::string_view text, const ParseContext &context,
        const ParseOptions &options = {}) const {
    ParseResult<std::shared_ptr<T>> result;
    CstBuilder builder(text);
    const auto input = builder.getText();
    detail::stepTraceReset();

#if defined(PEGIUM_BENCH_RECOVERY_ONLY)
    constexpr bool bypassInitialStrictParse = true;
#else
    constexpr bool bypassInitialStrictParse = false;
#endif

    std::size_t maxCursorOffset = 0;
    if constexpr (!bypassInitialStrictParse) {
      detail::stepTraceInc(detail::StepCounter::ParsePhaseRuns);
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
      std::size_t recoveryAttempt = 0;
      auto runRecoveryAttempt = [&](bool strictNoEdit, bool resetBuilder,
                                    std::size_t editFloorOffset,
                                    std::size_t editCeilingOffset)
          -> std::size_t {
        ++recoveryAttempt;
        detail::stepTraceInc(detail::StepCounter::RecoveryPhaseRuns);
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
        PEGIUM_RECOVERY_TRACE("[rule parse recover] ", getName(),
                              " attempt=", recoveryAttempt,
                              " floor=", editFloorOffset,
                              " ceil=", editCeilingOffset,
                              " strict=", strictNoEdit, " recoveredMatch=",
                              recoveredMatch, " len=", result.len, "/",
                              input.size(), " hadEdits=",
                              recoverState.hadEdits, " max=",
                              recoverState.maxCursorOffset(),
                              " diag=", result.diagnostics.size());
        return recoverState.maxCursorOffset();
      };

      if constexpr (bypassInitialStrictParse) {
        // First attempt in strict mode without resetting the fresh builder.
        const std::size_t strictMax =
            runRecoveryAttempt(/*strictNoEdit=*/true, /*resetBuilder=*/false,
                               /*editFloorOffset=*/0,
                               /*editCeilingOffset=*/input.size());
        if (!result.ret) {
          if (strictMax > maxCursorOffset) {
            maxCursorOffset = strictMax;
          }
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
        throw std::logic_error("ParserRule::parse matched node not found");
      }
      result.value = getTypedValue(*node);
    }

    detail::stepTraceDumpSummary(getName(), result.ret, result.recovered,
                                 result.len, input.size());

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
    PEGIUM_RECOVERY_TRACE("[rule recover] enter ", getName(), " offset=",
                          recoverState.cursorOffset(), " allowI=",
                          recoverState.allowInsert, " allowD=",
                          recoverState.allowDelete);
    const auto mark = recoverState.enter();
    if (!parse_assigned_recover(recoverState)) {
      PEGIUM_RECOVERY_TRACE("[rule recover] fail ", getName(), " offset=",
                            recoverState.cursorOffset());
      recoverState.rewind(mark);
      return false;
    }
    PEGIUM_RECOVERY_TRACE("[rule recover] ok ", getName(), " offset=",
                          recoverState.cursorOffset(), " hadEdits=",
                          recoverState.hadEdits);
    recoverState.exit(this);
    return true;
  }
  using BaseRule::operator=;
};
} // namespace pegium::parser
