/// `TerminalShape` vocabulary and legality tests.
///
/// Terminal recovery is normalised around an explicit `TerminalShape`
/// and closed legality predicates for `Replace`, `Insert`, and
/// `DeleteScan`. This suite covers:
///
///   1. `TerminalShape` builder produces correct shapes from
///      canonical text.
///   2. Legality predicates honour their documented inputs.
///   3. The fuzzy lookahead formula:
///      `maxLookahead = canonicalTextLength + affordableDeleteSpan`.
///   4. Four shape-driven scenarios:
///      - short literal stays conservative (single-codepoint
///        `Replace` rejected),
///      - long keyword fixed by replace when budget allows,
///      - fuzzy match rejected after invalid visible boundary,
///      - delete scan does not beat a better-contracted structured
///        insertion (ranking is via `RecoveryKey`, not the
///        predicates; verified via the predicates' input signatures
///        — `DeleteScan` reads only its own conditions and does not
///        read insertion facts).

#include <pegium/core/parser/TerminalShape.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using pegium::parser::detail::compute_terminal_max_lookahead;
using pegium::parser::detail::is_terminal_delete_scan_legal;
using pegium::parser::detail::is_terminal_insert_legal;
using pegium::parser::detail::is_terminal_replace_legal;
using pegium::parser::detail::make_terminal_shape_from_literal;
using pegium::parser::detail::TerminalLegalityFacts;
using pegium::parser::detail::TerminalShape;

// -----------------------------------------------------------------------------
// 1. Shape builder
// -----------------------------------------------------------------------------

TEST(TerminalShapeBuilder, empty_canonical_text_produces_default_shape) {
  const auto shape = make_terminal_shape_from_literal("", false, false);
  EXPECT_FALSE(shape.hasCanonicalText);
  EXPECT_EQ(shape.canonicalTextLength, 0U);
  EXPECT_FALSE(shape.singleCodepoint);
  EXPECT_FALSE(shape.boundarySensitive);
}

TEST(TerminalShapeBuilder, single_character_keyword_is_single_codepoint) {
  const auto shape =
      make_terminal_shape_from_literal(";", /*startsLikeWord=*/false,
                                        /*endsLikeWord=*/false);
  EXPECT_TRUE(shape.hasCanonicalText);
  EXPECT_EQ(shape.canonicalTextLength, 1U);
  EXPECT_TRUE(shape.singleCodepoint);
  EXPECT_FALSE(shape.boundarySensitive);
}

TEST(TerminalShapeBuilder, multi_character_word_keyword_is_boundary_sensitive) {
  const auto shape =
      make_terminal_shape_from_literal("service",
                                        /*startsLikeWord=*/true,
                                        /*endsLikeWord=*/true);
  EXPECT_TRUE(shape.hasCanonicalText);
  EXPECT_EQ(shape.canonicalTextLength, 7U);
  EXPECT_FALSE(shape.singleCodepoint);
  EXPECT_TRUE(shape.startsLikeWord);
  EXPECT_TRUE(shape.endsLikeWord);
  EXPECT_TRUE(shape.boundarySensitive);
}

TEST(TerminalShapeBuilder, punctuation_keyword_is_not_boundary_sensitive) {
  const auto shape =
      make_terminal_shape_from_literal("=>", /*startsLikeWord=*/false,
                                        /*endsLikeWord=*/false);
  EXPECT_TRUE(shape.hasCanonicalText);
  EXPECT_EQ(shape.canonicalTextLength, 2U);
  EXPECT_FALSE(shape.singleCodepoint);
  EXPECT_FALSE(shape.boundarySensitive);
}

TEST(TerminalShapeBuilder, struct_is_trivially_copyable_and_small) {
  static_assert(std::is_trivially_copyable_v<TerminalShape>);
  EXPECT_LE(sizeof(TerminalShape), 12U);
}

// -----------------------------------------------------------------------------
// 2. Replace legality
// -----------------------------------------------------------------------------

TEST(TerminalLegalityReplace, requires_canonical_text) {
  TerminalShape shape; // hasCanonicalText = false
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegalityReplace,
     single_codepoint_replacements_stay_conservative_rejected_here) {
  // Plan: "single-codepoint replacements restent conservateurs" —
  // they do NOT go through generic `Replace`. The shape's
  // `singleCodepoint` flag disqualifies the candidate.
  const auto shape = make_terminal_shape_from_literal(";", false, false);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegalityReplace, multi_codepoint_keyword_with_budget_admitted) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  EXPECT_TRUE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegalityReplace, no_budget_blocks_admission) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  // budgetAllowsReplace = false
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegalityReplace,
     boundary_sensitive_shape_with_violation_blocked) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  facts.fuzzyMatchViolatesLeadingBoundary = true;
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegalityReplace,
     non_boundary_sensitive_shape_ignores_boundary_violations) {
  // Punctuation keyword: not boundary-sensitive, so reported
  // boundary violations are irrelevant.
  const auto shape = make_terminal_shape_from_literal("=>", false, false);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  facts.fuzzyMatchViolatesLeadingBoundary = true;
  facts.fuzzyMatchViolatesTrailingBoundary = true;
  EXPECT_TRUE(is_terminal_replace_legal(shape, facts));
}

// -----------------------------------------------------------------------------
// 3. Insert legality
// -----------------------------------------------------------------------------

TEST(TerminalLegalityInsert, requires_continuation) {
  const auto shape = make_terminal_shape_from_literal(";", false, false);
  TerminalLegalityFacts facts;
  // tailOrFollowContinuationAvailable = false
  EXPECT_FALSE(is_terminal_insert_legal(shape, facts));
  facts.tailOrFollowContinuationAvailable = true;
  EXPECT_TRUE(is_terminal_insert_legal(shape, facts));
}

TEST(TerminalLegalityInsert, rejects_multi_character_literals_globally) {
  const auto shape = make_terminal_shape_from_literal("::", false, false);
  TerminalLegalityFacts facts;
  facts.tailOrFollowContinuationAvailable = true;
  EXPECT_FALSE(is_terminal_insert_legal(shape, facts));
}

TEST(TerminalLegalityInsert, requires_canonical_text) {
  TerminalShape shape; // no canonical text
  TerminalLegalityFacts facts;
  facts.tailOrFollowContinuationAvailable = true;
  EXPECT_FALSE(is_terminal_insert_legal(shape, facts));
}

// -----------------------------------------------------------------------------
// 4. DeleteScan legality
// -----------------------------------------------------------------------------

TEST(TerminalLegalityDeleteScan, requires_strict_terminal_or_follow_after_scan) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.budgetAllowsDeleteScan = true;
  // strictTerminalOrFollowAfterScan = false
  EXPECT_FALSE(is_terminal_delete_scan_legal(shape, facts));
  facts.strictTerminalOrFollowAfterScan = true;
  EXPECT_TRUE(is_terminal_delete_scan_legal(shape, facts));
}

TEST(TerminalLegalityDeleteScan, requires_budget) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.strictTerminalOrFollowAfterScan = true;
  // budgetAllowsDeleteScan = false
  EXPECT_FALSE(is_terminal_delete_scan_legal(shape, facts));
}

// -----------------------------------------------------------------------------
// 5. Fuzzy lookahead formula
// -----------------------------------------------------------------------------

TEST(TerminalFuzzyLookahead,
     formula_is_canonical_length_plus_affordable_delete_span) {
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  EXPECT_EQ(compute_terminal_max_lookahead(shape, 4U), 11U);
  EXPECT_EQ(compute_terminal_max_lookahead(shape, 0U), 7U);
}

TEST(TerminalFuzzyLookahead,
     no_canonical_text_yields_zero_window) {
  TerminalShape shape; // no canonical text
  EXPECT_EQ(compute_terminal_max_lookahead(shape, 100U), 0U);
}

// -----------------------------------------------------------------------------
// 6. Shape-driven scenarios
// -----------------------------------------------------------------------------

TEST(TerminalLegality, short_literal_conservative_replace_rejected) {
  // A 1-character keyword stays conservative: it cannot be replaced
  // via the generic fuzzy `Replace` path.
  const auto shape = make_terminal_shape_from_literal(":", false, false);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegality, long_keyword_replace_admissible_when_budget_allows) {
  // A long keyword is fixable by `Replace` when the budget allows.
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  EXPECT_TRUE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegality, fuzzy_rejected_after_visible_boundary_invalid) {
  // A fuzzy match is rejected when a visible boundary is invalid.
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts facts;
  facts.budgetAllowsReplace = true;
  facts.fuzzyMatchViolatesTrailingBoundary = true;
  EXPECT_FALSE(is_terminal_replace_legal(shape, facts));
}

TEST(TerminalLegality, delete_scan_does_not_consume_insert_or_replace_facts) {
  // The legality predicate for `DeleteScan` reads ONLY its own
  // conditions — it does not consume the insertion continuation fact
  // or the replace boundary facts. Ranking between admissible
  // candidates is then arbitrated by `RecoveryKey` (cost-penalised
  // first-edit offset). This test verifies the structural
  // decoupling: the `DeleteScan` predicate's truth value is
  // invariant under changes to insertion / replace facts.
  const auto shape = make_terminal_shape_from_literal("service", true, true);
  TerminalLegalityFacts factsBase;
  factsBase.strictTerminalOrFollowAfterScan = true;
  factsBase.budgetAllowsDeleteScan = true;
  TerminalLegalityFacts factsWithInsert = factsBase;
  factsWithInsert.tailOrFollowContinuationAvailable = true;
  TerminalLegalityFacts factsWithReplaceBudget = factsBase;
  factsWithReplaceBudget.budgetAllowsReplace = true;
  TerminalLegalityFacts factsWithBoundaryViolation = factsBase;
  factsWithBoundaryViolation.fuzzyMatchViolatesLeadingBoundary = true;

  EXPECT_TRUE(is_terminal_delete_scan_legal(shape, factsBase));
  EXPECT_TRUE(is_terminal_delete_scan_legal(shape, factsWithInsert));
  EXPECT_TRUE(is_terminal_delete_scan_legal(shape, factsWithReplaceBudget));
  EXPECT_TRUE(
      is_terminal_delete_scan_legal(shape, factsWithBoundaryViolation));
}
