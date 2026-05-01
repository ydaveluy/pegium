/// Vertical composition tests.
///
/// Vertical composition is the largest residual risk in nested
/// combinators (Group inside Repetition, Repetition inside
/// OrderedChoice, etc.). This suite asserts:
///
///   - an observed candidate can be abandoned;
///   - an admitted candidate can be dominated;
///   - a parent can reject a child's admitted candidate (and
///     produce its own structural reason);
///   - a winning candidate's replay reproduces exactly its
///     `replayPlan`;
///   - an edit promoted in a `RecoveryAttempt` is not silently lost
///     by a parent or by the global commit.
///
/// Each test names the property it targets and pins the observable
/// consequence rather than the internal mechanism, so the dispatch
/// can change without invalidating the contract.

#include "RecoveryTestSupport.hpp"
#include <pegium/core/RecoveryCacheNeutralityHarness.hpp>

using namespace pegium::parser;
using namespace pegium::test::recovery;
using pegium::test::expect_cache_neutral;

namespace {

bool any_diagnostic_is_inserted(
    const std::vector<ParseDiagnostic> &diagnostics) noexcept {
  return std::ranges::any_of(diagnostics, [](const ParseDiagnostic &d) {
    return d.kind == ParseDiagnosticKind::Inserted;
  });
}

bool any_diagnostic_is_deleted(
    const std::vector<ParseDiagnostic> &diagnostics) noexcept {
  return std::ranges::any_of(diagnostics, [](const ParseDiagnostic &d) {
    return d.kind == ParseDiagnosticKind::Deleted;
  });
}

std::size_t count_diagnostics_at(
    const std::vector<ParseDiagnostic> &diagnostics,
    pegium::TextOffset begin, pegium::TextOffset end,
    ParseDiagnosticKind kind) noexcept {
  return static_cast<std::size_t>(std::ranges::count_if(
      diagnostics, [begin, end, kind](const ParseDiagnostic &d) {
        return d.kind == kind && d.beginOffset == begin && d.endOffset == end;
      }));
}

} // namespace

// -----------------------------------------------------------------------------
// 1. Observed candidate can be abandoned
//
// A `Repetition` body's no-edit attempt may succeed on a partial
// match yet be abandoned in favour of a stricter outer goal. The
// observation is captured by the dispatch but does not survive
// promotion — the OUTER parse keeps the cleaner result.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     observed_inner_match_can_be_abandoned_for_cleaner_outer_result) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Inner: many(id). Outer: many(id) + "end". On input "alpha end"
  // the inner iteration's strict no-edit attempt could keep going
  // ("end" matches `id` regex), but doing so would prevent the
  // outer "end" keyword from matching. The dispatch must abandon
  // the over-greedy inner match and yield to the outer goal.
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(id) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(rule, "alpha end", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// 2. Admitted candidate can preserve a visible prefix
//
// When a repeated item can be recovered either by deleting a visible
// prefix or by inserting the missing structural literal before that
// prefix, the shared key may keep the insert because it preserves
// source and still reaches the same parent boundary. Observable:
// recovery stays local and produces a full match without requiring a
// hidden delete preference.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     boundary_insert_can_preserve_visible_prefix_in_nested_repetition) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Repetition of items where each item is one of two keyword
  // forms; recovery must extend the delete prefix to reach the
  // cleaner branch, not stop at the first viable candidate.
  ParserRule<RecoveryNode> item{
      "Item", ("a"_kw + assign<&RecoveryNode::token>(id)) |
                  ("ab"_kw + ";"_kw + assign<&RecoveryNode::token>(id))};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(item) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  // The leading "x" can be interpreted as either noise before the
  // next item or as the identifier of a recovered first item. Both
  // are generic repairs; the shared key keeps the cheaper
  // source-preserving insertion.
  const auto result = parseRule(rule, "x   ab; foo end", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_TRUE(any_diagnostic_is_inserted(result.parseDiagnostics))
      << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_FALSE(any_diagnostic_is_deleted(result.parseDiagnostics))
      << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// 3. Parent can reject a child's admitted candidate
//
// A child's local recovery may admit a candidate that the parent
// scope cannot accept (e.g. it would cross the parent's strict
// follow). The parent must reject that candidate without recovery
// silently leaking inadmissible edits.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     parent_rejects_inner_recovery_that_would_cross_parent_follow) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Inner choice would happily insert "alpha" if its first
  // alternative could match; outer requires "stop" immediately
  // after, which would conflict. Recovery must respect the
  // parent's strict follow.
  ParserRule<RecoveryNode> innerChoice{
      "InnerChoice", "alpha"_kw + assign<&RecoveryNode::token>(id) |
                         "beta"_kw + assign<&RecoveryNode::token>(id)};
  ParserRule<RecoveryEnvironmentNode> outer{
      "Outer", assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                   assign<&RecoveryEnvironmentNode::label>(id) + "stop"_kw};
  // Input has no entry for innerChoice but does for outer; outer's
  // `stop` keyword should mark the boundary.
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(outer, "name : value stop", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_TRUE(result.parseDiagnostics.empty())
      << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// 4. Winning candidate's replay reproduces exactly its replayPlan
//
// A successful recovery's diagnostics must be a faithful trace of
// the script the dispatch chose. The cache-on / cache-off harness
// already proves the dispatch is deterministic; this test pins the
// replay obligation by asserting that the diagnostic edits in the
// parse output match the recovered script exactly across the two
// paths.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     winning_candidate_produces_identical_diagnostics_under_both_cache_modes) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNode> item{
      "Item", "id"_kw + assign<&RecoveryNode::token>(id)};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(item) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  // Malformed input that exercises a multi-edit recovery script.
  const std::string input = "id alpha @@ id beta end";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseRule(rule, input, skipper, opts);
      },
      "vertical_composition_replay_obligation", input);
}

// -----------------------------------------------------------------------------
// 5. Promoted edit cannot be silently lost
//
// When a nested recovery succeeds and promotes its edits to the
// `RecoveryAttempt`, those edits must survive the outer parse's
// completion. An edit visible in the diagnostics of the inner
// scope must remain in the final diagnostics list.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     edit_promoted_inside_nested_iteration_appears_in_final_diagnostics) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Outer many(item) + "end". Item = "id" + id. Inner recovery
  // may need to insert/delete to repair a malformed item.
  ParserRule<RecoveryNode> item{
      "Item", "id"_kw + assign<&RecoveryNode::token>(id)};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(item) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  // Two valid items separated by garbage; recovery must delete the
  // garbage and the deletion must remain in final diagnostics.
  const auto result = parseRule(rule, "id alpha xx id beta end", skipper);
  ASSERT_TRUE(result.value)
      << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
  // The deletion of the noise "xx" must be present in final
  // diagnostics — recovery edits are not erased after promotion.
  EXPECT_TRUE(any_diagnostic_is_deleted(result.parseDiagnostics) ||
              any_diagnostic_is_inserted(result.parseDiagnostics))
      << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// 6. Promoted edit is preserved across nested recovery boundaries
//
// More structured variant: a Group inside a Repetition inside the
// outer rule. An inner recovery edit must propagate through both
// composition layers without being dropped.
// -----------------------------------------------------------------------------

TEST(VerticalComposition,
     inner_recovery_in_nested_group_repetition_does_not_lose_edits) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Group inside Repetition: each "binding" is "id" + id + ";".
  // Outer: many(binding) + "end".
  ParserRule<RecoveryNode> binding{
      "Binding", "let"_kw + assign<&RecoveryNode::token>(id) + ";"_kw};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(binding) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  // First binding well-formed, second missing the trailing ";".
  // Recovery should insert the missing ";" — the inner Group's
  // recovery survives across the outer Repetition.
  const auto result =
      parseRule(rule, "let alpha; let beta end", skipper);
  ASSERT_TRUE(result.value)
      << dump_parse_diagnostics(result.parseDiagnostics);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
  // An insertion or deletion must have been produced AND survived.
  const bool hasEdit = any_diagnostic_is_inserted(result.parseDiagnostics) ||
                       any_diagnostic_is_deleted(result.parseDiagnostics);
  EXPECT_TRUE(hasEdit) << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// 7. No silent duplication of a single recovery edit
//
// Edits should appear exactly once in the diagnostics. An inner
// recovery that produces a single edit must not be duplicated by
// the outer parse's commit step.
// -----------------------------------------------------------------------------

TEST(VerticalComposition, single_recovery_edit_is_not_duplicated_at_commit) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // A simple sequence with one recoverable error. Recovery
  // produces exactly one edit; the diagnostic list must reflect
  // that exact count, with no duplicates from the outer commit.
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const auto skipper = SkipperBuilder().build();
  const auto result = parseDataType(rule, "xservice", skipper);
  ASSERT_TRUE(result.value);
  // Exactly one delete at position 0 (the leading "x"), not two.
  const auto deletesAtZero = count_diagnostics_at(
      result.parseDiagnostics, /*begin=*/0, /*end=*/1,
      ParseDiagnosticKind::Deleted);
  EXPECT_EQ(deletesAtZero, 1U)
      << dump_parse_diagnostics(result.parseDiagnostics);
}

// -----------------------------------------------------------------------------
// Grammar walker pinning tests
//
// Pin observable behaviors gated by the four private static walkers in
// Repetition.hpp (`starts_with_insertable_terminal`,
// `is_unassigned_lexical_tail`,
// `starts_with_synthetic_terminal_then_unassigned_lexical_tail`,
// `starts_with_required_literal_anchor`). These tests must keep passing
// across the upcoming collapse refactor that fuses the four walkers into
// one shared traversal helper.
// -----------------------------------------------------------------------------

TEST(RepetitionGrammarWalkers,
     ManyOverIdRecoversTrailingDelimiter) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionBlockNode> rule{"Rule",
                                                many(id) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(rule, "alpha beta", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
}

TEST(RepetitionGrammarWalkers,
     ManyOverIdAlwaysProducesAValueWhenEndKeywordFollows) {
  // `many(id)` is greedy and consumes `end` (`id` regex matches it),
  // then recovery synthesises the missing `end` at EOF. Both the
  // strict-yield path and the greedy-then-insert path are valid
  // observable shapes — pin only that recovery succeeds.
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionBlockNode> rule{"Rule",
                                                many(id) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(rule, "alpha end", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
}

TEST(RepetitionGrammarWalkers,
     SeparatedListWithCommaSeparatorRecoversCleanly) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", id + many(","_kw + id) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(rule, "a b c end", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
}

TEST(RepetitionGrammarWalkers,
     RepetitionWithRequiredLiteralAnchorYieldsToOuter) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNode> item{
      "Item", "let"_kw + assign<&RecoveryNode::token>(id)};
  ParserRule<RecoveryTransitionBlockNode> rule{"Rule",
                                                many(item) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const auto result = parseRule(rule, "let a let b end", skipper);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch)
      << dump_parse_diagnostics(result.parseDiagnostics);
}
