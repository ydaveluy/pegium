#pragma once
#include <algorithm>
#include <cassert>
#include <iostream>
#include <pegium/grammar/Action.hpp>
#include <pegium/grammar/Assignment.hpp>
#include <pegium/parser/AbstractRule.hpp>
#include <pegium/parser/Action.hpp>
#include <pegium/parser/Assignment.hpp>
#include <pegium/parser/CstSearch.hpp>
#include <pegium/parser/IParser.hpp>
#include <pegium/parser/Introspection.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/parser/RecoveryTrace.hpp>
#include <pegium/parser/Skipper.hpp>
#include <pegium/parser/StepTrace.hpp>
#include <stdexcept>
#include <string_view>

namespace pegium::parser {

template <typename T>
  requires std::derived_from<T, AstNode>
struct ParserRule final : AbstractRule<grammar::ParserRule> {
  using type = T;
  using BaseRule = AbstractRule<grammar::ParserRule>;
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
      NodeId node;
    };
    std::vector<AssignmentEntry> assignments;

    const auto &root = node.root();

    for (const auto view : node) {
      const auto &n = view.node();
      if (n.isHidden) {
        continue;
      }
      assert(n.grammarElement);
      switch (n.grammarElement->getKind()) {
      case ElementKind::Assignment: {
        assignments.emplace_back(
            static_cast<const grammar::Assignment *>(n.grammarElement),
            view.id());
        break;
      }
      case ElementKind::New: {
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::Action *>(n.grammarElement)
                ->execute(current));
        break;
      }
      case ElementKind::Init: {
        if (!current) {
          current = std::make_shared<T>();
        }
        for (const auto &entry : assignments) {
          entry.assignment->execute(current.get(),
                                    CstNodeView(&root, entry.node));
        }
        assignments.clear();
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::Action *>(n.grammarElement)
                ->execute(current));
        break;
      }
      case ElementKind::ParserRule: {
        current = std::static_pointer_cast<T>(
            static_cast<const grammar::ParserRule *>(n.grammarElement)
                ->getValue(view));
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
      entry.assignment->execute(current.get(), CstNodeView(&root, entry.node));
    }
    return current;
  }
  GenericParseResult parseGeneric(std::string_view text, const Skipper &context,
                                  const ParseOptions &options = {}) const {
    auto result = parse(text, context, options);
    return {.root_node = result.root_node};
  }
  ParseResult<std::shared_ptr<T>>
  parse(std::string_view text, const Skipper &skipper,
        const ParseOptions &options = {}) const {
    ParseResult<std::shared_ptr<T>> result;
    CstBuilder builder(text);
    const auto input = builder.getText();
    detail::stepTraceReset();

    std::uint32_t maxCursorOffset = 0;

    std::uint32_t recoveryAttempt = 0;
    auto runRecoveryAttempt = [&](bool strictNoEdit, bool resetBuilder,
                                  std::uint32_t editFloorOffset,
                                  std::uint32_t editCeilingOffset) {
      ++recoveryAttempt;
      detail::stepTraceInc(detail::StepCounter::RecoveryPhaseRuns);
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
      PEGIUM_RECOVERY_TRACE(
          "[rule parse rule] ", getName(), " attempt=", recoveryAttempt,
          " floor=", editFloorOffset, " ceil=", editCeilingOffset,
          " strict=", strictNoEdit, " recoveredMatch=", recoveredMatch,
          " len=", result.len, "/", input.size(),
          " hadEdits=", context.hadEdits, " max=", context.maxCursorOffset(),
          " diag=", result.diagnostics.size());
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
        throw std::logic_error("ParserRule::parse matched node not found");
      result.value = getTypedValue(*node);
    }

    detail::stepTraceDumpSummary(getName(), result.ret, result.recovered,
                                 result.len, input.size());
    return result;
  }

  bool rule(ParseContext &ctx) const {
    PEGIUM_RECOVERY_TRACE(
        "[rule rule] enter ", getName(), " offset=", ctx.cursorOffset(),
        " allowI=", ctx.allowInsert, " allowD=", ctx.allowDelete);
    const auto mark = ctx.enter();
    if (!rule_fast(ctx)) {
      PEGIUM_RECOVERY_TRACE("[rule rule] fail ", getName(),
                            " offset=", ctx.cursorOffset());
      ctx.rewind(mark);
      return false;
    }
    PEGIUM_RECOVERY_TRACE("[rule rule] ok ", getName(),
                          " offset=", ctx.cursorOffset(),
                          " hadEdits=", ctx.hadEdits);
    ctx.exit(mark, this);
    return true;
  }
  using BaseRule::operator=;
};
} // namespace pegium::parser
