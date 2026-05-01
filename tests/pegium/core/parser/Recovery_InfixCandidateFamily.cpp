/// `InfixCandidateFamily` vocabulary and legality tests.
///
/// `InfixRule` recovery is framed as three closed candidate families
/// plus a local `LocalRhsNoiseCleanup` contract. This suite covers
/// the vocabulary and the legality predicates that `InfixRule.hpp`
/// dispatch reads:
///
///   1. `InfixCandidateFamily` enum is closed at 3 documented
///      values.
///   2. `InfixCandidateFamilyFacts` defaults to all-false.
///   3. Each family's legality predicate reads only its
///      documented facts.
///   4. The dispatch is total over the closed enum.
///   5. `LocalRhsNoiseCleanup` requires all three obligations to
///      be admissible.
///   6. Two non-negotiable rules:
///      - operator never synthesised: `ContinueTail` requires a
///        strict operator (a recoverable-only operator does not
///        promote `ContinueTail`).
///      - delete scan refused if no primary strict follows:
///        `DeleteOperatorNoise` requires
///        `strictRhsPrimaryAfterNoiseDelete`.

#include <pegium/core/parser/InfixRule.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using pegium::parser::detail::InfixCandidateFamily;
using pegium::parser::detail::InfixCandidateFamilyFacts;
using pegium::parser::detail::infix_candidate_family_name;
using pegium::parser::detail::is_continue_tail_legal;
using pegium::parser::detail::is_delete_operator_noise_legal;
using pegium::parser::detail::is_infix_candidate_family_legal;
using pegium::parser::detail::is_local_rhs_noise_cleanup_admissible;
using pegium::parser::detail::is_stop_tail_legal;
using pegium::parser::detail::LocalRhsNoiseCleanup;

// -----------------------------------------------------------------------------
// 1. Closed enum
// -----------------------------------------------------------------------------

TEST(InfixCandidateFamilyEnum, family_values_are_documented_and_closed) {
  for (const auto value : {InfixCandidateFamily::StopTail,
                            InfixCandidateFamily::ContinueTail,
                            InfixCandidateFamily::DeleteOperatorNoise}) {
    switch (value) {
    case InfixCandidateFamily::StopTail:
    case InfixCandidateFamily::ContinueTail:
    case InfixCandidateFamily::DeleteOperatorNoise:
      SUCCEED();
      break;
    }
  }
}

TEST(InfixCandidateFamilyEnum, names_are_stable) {
  EXPECT_STREQ(infix_candidate_family_name(InfixCandidateFamily::StopTail),
               "StopTail");
  EXPECT_STREQ(infix_candidate_family_name(InfixCandidateFamily::ContinueTail),
               "ContinueTail");
  EXPECT_STREQ(
      infix_candidate_family_name(InfixCandidateFamily::DeleteOperatorNoise),
      "DeleteOperatorNoise");
}

// -----------------------------------------------------------------------------
// 2. Facts struct shape
// -----------------------------------------------------------------------------

TEST(InfixCandidateFamilyFactsStruct, default_facts_are_all_false) {
  const InfixCandidateFamilyFacts facts;
  EXPECT_FALSE(facts.strictOperatorAtCursor);
  EXPECT_FALSE(facts.recoverableOperatorAtCursor);
  EXPECT_FALSE(facts.parentFollowStrict);
  EXPECT_FALSE(facts.strictRhsPrimaryAfterOperator);
  EXPECT_FALSE(facts.recoverableRhsPrimaryAfterOperator);
  EXPECT_FALSE(facts.strictRhsPrimaryAfterNoiseDelete);
  EXPECT_FALSE(facts.operatorNoiseDeleteBudgetAvailable);
}

TEST(InfixCandidateFamilyFactsStruct, struct_is_trivially_copyable_and_small) {
  static_assert(std::is_trivially_copyable_v<InfixCandidateFamilyFacts>);
  EXPECT_LE(sizeof(InfixCandidateFamilyFacts), 8U);
}

// -----------------------------------------------------------------------------
// 3. Per-family legality
// -----------------------------------------------------------------------------

TEST(InfixCandidateFamilyLegality,
     stop_tail_legal_when_no_strict_operator_at_cursor) {
  InfixCandidateFamilyFacts facts;
  EXPECT_TRUE(is_stop_tail_legal(facts));
  facts.recoverableOperatorAtCursor = true;
  // Recoverable-only operator does not block stop-tail (operator
  // never synthesised, so without strict accept there's nothing
  // to commit to).
  EXPECT_TRUE(is_stop_tail_legal(facts));
  facts.strictOperatorAtCursor = true;
  EXPECT_FALSE(is_stop_tail_legal(facts));
}

TEST(InfixCandidateFamilyLegality,
     continue_tail_requires_strict_operator) {
  InfixCandidateFamilyFacts facts;
  facts.strictRhsPrimaryAfterOperator = true;
  // Strict RHS alone is not enough — operator must be strict.
  EXPECT_FALSE(is_continue_tail_legal(facts));
  facts.strictOperatorAtCursor = true;
  EXPECT_TRUE(is_continue_tail_legal(facts));
  // Recoverable RHS also works once strict operator is present.
  facts.strictRhsPrimaryAfterOperator = false;
  facts.recoverableRhsPrimaryAfterOperator = true;
  EXPECT_TRUE(is_continue_tail_legal(facts));
}

TEST(InfixCandidateFamilyLegality,
     continue_tail_requires_some_rhs_primary) {
  InfixCandidateFamilyFacts facts;
  facts.strictOperatorAtCursor = true;
  // No RHS primary at all — illegal.
  EXPECT_FALSE(is_continue_tail_legal(facts));
}

TEST(InfixCandidateFamilyLegality,
     delete_operator_noise_requires_strict_rhs_primary_after_scan) {
  InfixCandidateFamilyFacts facts;
  facts.operatorNoiseDeleteBudgetAvailable = true;
  // Without strict RHS after scan: illegal.
  EXPECT_FALSE(is_delete_operator_noise_legal(facts));
  facts.strictRhsPrimaryAfterNoiseDelete = true;
  EXPECT_TRUE(is_delete_operator_noise_legal(facts));
}

TEST(InfixCandidateFamilyLegality,
     delete_operator_noise_requires_budget) {
  InfixCandidateFamilyFacts facts;
  facts.strictRhsPrimaryAfterNoiseDelete = true;
  // Without budget: illegal.
  EXPECT_FALSE(is_delete_operator_noise_legal(facts));
  facts.operatorNoiseDeleteBudgetAvailable = true;
  EXPECT_TRUE(is_delete_operator_noise_legal(facts));
}

// -----------------------------------------------------------------------------
// 4. Dispatch is total over the closed enum
// -----------------------------------------------------------------------------

TEST(InfixCandidateFamilyLegality, dispatch_calls_each_predicate) {
  InfixCandidateFamilyFacts facts;
  facts.strictOperatorAtCursor = true;
  facts.strictRhsPrimaryAfterOperator = true;
  facts.strictRhsPrimaryAfterNoiseDelete = true;
  facts.operatorNoiseDeleteBudgetAvailable = true;
  // StopTail not legal because strict operator is present.
  EXPECT_FALSE(
      is_infix_candidate_family_legal(InfixCandidateFamily::StopTail, facts));
  EXPECT_TRUE(is_infix_candidate_family_legal(
      InfixCandidateFamily::ContinueTail, facts));
  EXPECT_TRUE(is_infix_candidate_family_legal(
      InfixCandidateFamily::DeleteOperatorNoise, facts));
}

TEST(InfixCandidateFamilyLegality, default_facts_admit_only_stop_tail) {
  const InfixCandidateFamilyFacts facts;
  EXPECT_TRUE(
      is_infix_candidate_family_legal(InfixCandidateFamily::StopTail, facts));
  EXPECT_FALSE(is_infix_candidate_family_legal(
      InfixCandidateFamily::ContinueTail, facts));
  EXPECT_FALSE(is_infix_candidate_family_legal(
      InfixCandidateFamily::DeleteOperatorNoise, facts));
}

// -----------------------------------------------------------------------------
// 5. LocalRhsNoiseCleanup contract
// -----------------------------------------------------------------------------

TEST(LocalRhsNoiseCleanupContract, default_contract_is_inadmissible) {
  const LocalRhsNoiseCleanup contract;
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
}

TEST(LocalRhsNoiseCleanupContract, all_three_obligations_required) {
  LocalRhsNoiseCleanup contract;
  contract.respectsObservationObligation = true;
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
  contract.respectsReplayObligation = true;
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
  contract.policyMutationVisibleInFingerprint = true;
  EXPECT_TRUE(is_local_rhs_noise_cleanup_admissible(contract));
}

TEST(LocalRhsNoiseCleanupContract,
     missing_observation_obligation_blocks_admission) {
  LocalRhsNoiseCleanup contract;
  contract.respectsReplayObligation = true;
  contract.policyMutationVisibleInFingerprint = true;
  // Missing observation: still inadmissible.
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
}

TEST(LocalRhsNoiseCleanupContract,
     missing_replay_obligation_blocks_admission) {
  LocalRhsNoiseCleanup contract;
  contract.respectsObservationObligation = true;
  contract.policyMutationVisibleInFingerprint = true;
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
}

TEST(LocalRhsNoiseCleanupContract,
     missing_policy_fingerprint_visibility_blocks_admission) {
  LocalRhsNoiseCleanup contract;
  contract.respectsObservationObligation = true;
  contract.respectsReplayObligation = true;
  EXPECT_FALSE(is_local_rhs_noise_cleanup_admissible(contract));
}

// -----------------------------------------------------------------------------
// 6. Plan obligations as predicate-level invariants
// -----------------------------------------------------------------------------

TEST(InfixCandidateFamilyLegality,
     operator_never_synthesised_recoverable_only_does_not_promote_continue) {
  // The operator is never synthesised by recovery: a recoverable-only
  // operator must NOT promote `ContinueTail`.
  InfixCandidateFamilyFacts facts;
  facts.recoverableOperatorAtCursor = true;
  facts.strictRhsPrimaryAfterOperator = true;
  EXPECT_FALSE(is_continue_tail_legal(facts));
}

TEST(InfixCandidateFamilyLegality,
     delete_scan_refused_when_no_primary_strict_follows) {
  // The scan is refused if no strict primary follows it.
  InfixCandidateFamilyFacts facts;
  facts.operatorNoiseDeleteBudgetAvailable = true;
  facts.recoverableRhsPrimaryAfterOperator = true; // not enough
  EXPECT_FALSE(is_delete_operator_noise_legal(facts));
}
