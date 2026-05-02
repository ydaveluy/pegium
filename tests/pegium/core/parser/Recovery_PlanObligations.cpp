/// Integration assertions for invariants that have no predicate-level
/// equivalent. The bulk of the closed-vocabulary tests live in
/// `Recovery_TerminalShape.cpp`, `Recovery_InfixCandidateFamily.cpp`
/// and `Recovery_GroupTransition.cpp`; the two scenarios in this file
/// exercise the dispatch wiring end-to-end on a small grammar.

#include "RecoveryTestSupport.hpp"

#include <gtest/gtest.h>

using namespace pegium::parser;
using namespace pegium::test::recovery;

// A delete-run on a terminal must not cross a protected boundary.
//
// `Group::observe_current_terminal_delete_run` and
// `Group::replay_current_terminal_delete_run` configure the scan
// with `stopAtHiddenTriviaBoundary = true` and
// `allowOverflow = false`, which guarantees the run terminates at
// the first protected boundary (a hidden-trivia boundary in the
// current model). This test pins the integration consequence: a
// delete-run that would have to cross a hidden whitespace boundary
// to reach a strict terminal target stops short — the recovery
// either takes the boundary as a stop, or fails the local repair
// and lets the parser fall back to a different transition.
TEST(GroupRecovery, delete_run_terminal_respects_hidden_trivia_boundary) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Grammar: a sequence of `def NAME ;`. The dispatch at the `;`
  // terminal must not delete-run across the ID's whitespace
  // boundary — doing so would steal a leaf from the next iteration.
  ParserRule<RecoveryDefinition> definition{
      "Definition",
      "def"_kw + create<RecoveryDefinition>() +
          assign<&RecoveryDefinition::name>(id) + ";"_kw};
  ParserRule<RecoveryModule> entry{
      "Module",
      pegium::parser::create<RecoveryModule>() +
          some(append<&RecoveryModule::statements>(definition))};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  // Input: a malformed `def x ?? def y ;` where `??` is a delete-run
  // candidate AT the boundary between the two `def` items. The
  // recovery must NOT consume the second `def` keyword in a
  // delete-run — that would commit a deletion across the
  // boundary protecting the next iteration.
  const auto result = parseRule(entry, "def x ?? def y;", skipper);
  ASSERT_TRUE(result.value);
  const auto *module = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(module, nullptr);
  // The second definition's name must still be `y`. If the
  // delete-run had crossed the boundary it would have absorbed the
  // `def` keyword and the second iteration would never have started
  // (or would have started on `y;` with a missing keyword).
  ASSERT_GE(module->statements.size(), 2U);
  const auto *firstDef =
      dynamic_cast<RecoveryDefinition *>(module->statements[0]);
  const auto *secondDef =
      dynamic_cast<RecoveryDefinition *>(module->statements[1]);
  ASSERT_NE(firstDef, nullptr);
  ASSERT_NE(secondDef, nullptr);
  EXPECT_EQ(firstDef->name, "x");
  EXPECT_EQ(secondDef->name, "y");
}

// No recovery edit may land before `rhsStart`.
//
// `InfixRule` recovery captures its candidate's `firstEditOffset` at
// `observation.strayOperatorRunFirstEditOffset` (the operator
// position when the recovery was observed), not anywhere inside
// the LHS. End-to-end consequence: when a clean LHS parses
// successfully and only the RHS / operator side requires recovery,
// the diagnostics carry edits at or after the LHS end.
TEST(InfixRecovery, no_recovery_edit_lands_before_rhs_start_offset) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
             &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                  LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation", option(assign<&RecoveryExpressionEvaluation::expression>(
                        expressionRule)) +
                        ";"_kw};
  // Input `81/;`: clean LHS `81`, stray `/` at offset 2, then `;`.
  // The recovery must produce its Deleted edit at offset >= 2 (the
  // operator position), not inside the LHS.
  const auto result = parseRule(evaluation, "81/;", skipper);
  ASSERT_TRUE(result.value)
      << dump_parse_diagnostics(result.parseDiagnostics);
  for (const auto &d : result.parseDiagnostics) {
    if (d.kind != ParseDiagnosticKind::Deleted &&
        d.kind != ParseDiagnosticKind::Inserted) {
      continue;
    }
    // No edit before the operator's start position (offset 2).
    EXPECT_GE(d.offset, 2U)
        << "edit at offset " << d.offset
        << " predates LHS end (rhsStart >= 2): "
        << dump_parse_diagnostics(result.parseDiagnostics);
  }
}
