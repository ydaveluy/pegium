/// `IterationBoundaryDecision` precedence table tests.
///
/// `Repetition` boundary precedence is a closed table on 6 boolean
/// facts. The table is total and exclusive: every one of the 64
/// combinations receives exactly one decision; combinations declared
/// impossible by construction live in
/// `kImpossibleIterationFactCombinations` with their justification.
///
/// This suite covers:
///
///   1. Exhaustivity: all 64 combinations enumerated; each receives
///      a single decision.
///   2. Each rule of the closed precedence table fires for at least
///      one combination (no dead rule).
///   3. The closed list of impossible combinations is honoured.
///   4. Five integration scenarios:
///        - a fresh option does not synthesise without an entry signal;
///        - strict parent follow wins over first recoverable;
///        - strict continuation wins explicitly over stop-on-strict-
///          follow when both are applicable;
///        - delete retry does not start a hypothetical iteration;
///        - resync only wins as a last resort.

#include <pegium/core/parser/IterationBoundaryDecision.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <set>

using pegium::parser::detail::decide_iteration_boundary;
using pegium::parser::detail::is_impossible_iteration_fact_combination;
using pegium::parser::detail::IterationBoundaryDecision;
using pegium::parser::detail::IterationBoundaryFacts;
using pegium::parser::detail::kImpossibleIterationFactCombinations;

namespace {

constexpr IterationBoundaryFacts make_facts(std::uint8_t mask) noexcept {
  return IterationBoundaryFacts{
      .startedStrictly = (mask & 0b000001U) != 0U,
      .firstStrict = (mask & 0b000010U) != 0U,
      .firstRecoverable = (mask & 0b000100U) != 0U,
      .followStrict = (mask & 0b001000U) != 0U,
      .followRecoverable = (mask & 0b010000U) != 0U,
      .committedPrefixImposes = (mask & 0b100000U) != 0U,
  };
}

} // namespace

// -----------------------------------------------------------------------------
// 1. Exhaustivity: 64 combinations, each receives exactly one decision
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence,
     all_64_combinations_receive_exactly_one_decision) {
  std::array<int, 6> decisionCounts{};
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    const auto facts = make_facts(mask);
    const auto decision = decide_iteration_boundary(facts);
    // Coverage assertion: the decision is one of the 6 closed values.
    switch (decision) {
    case IterationBoundaryDecision::StopCleanly:
    case IterationBoundaryDecision::ContinueStrict:
    case IterationBoundaryDecision::ContinueRecoverable:
    case IterationBoundaryDecision::RepairStartedIteration:
    case IterationBoundaryDecision::Resync:
    case IterationBoundaryDecision::RejectToParentFollow:
      ++decisionCounts[static_cast<std::size_t>(decision)];
      break;
    }
  }
  // Sanity: the total counted equals 64 (no enum value escaped the
  // switch).
  int total = 0;
  for (int count : decisionCounts) {
    total += count;
  }
  EXPECT_EQ(total, 64);
}

// -----------------------------------------------------------------------------
// 2. Each rule fires at least once (no dead rule)
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, every_decision_value_is_reachable) {
  std::set<IterationBoundaryDecision> seen;
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    seen.insert(decide_iteration_boundary(make_facts(mask)));
  }
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::StopCleanly));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::ContinueStrict));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::ContinueRecoverable));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::RepairStartedIteration));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::Resync));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::RejectToParentFollow));
  EXPECT_EQ(seen.size(), 6U) << "unexpected new decision value";
}

// -----------------------------------------------------------------------------
// 3. Closed list of impossible combinations
// -----------------------------------------------------------------------------

TEST(IterationBoundaryImpossible,
     impossible_combinations_match_documented_patterns) {
  // The two documented impossibility patterns.
  EXPECT_EQ(std::size(kImpossibleIterationFactCombinations), 2U);

  // first_strict_implies_first_recoverable: any input with FS=T and
  // FR=F is flagged as impossible regardless of other facts.
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    const auto facts = make_facts(mask);
    if (facts.firstStrict && !facts.firstRecoverable) {
      EXPECT_TRUE(is_impossible_iteration_fact_combination(facts))
          << "FS=T, FR=F should be impossible (mask=" << static_cast<int>(mask)
          << ")";
    }
    if (facts.followStrict && !facts.followRecoverable) {
      EXPECT_TRUE(is_impossible_iteration_fact_combination(facts))
          << "FoS=T, FoR=F should be impossible (mask=" << static_cast<int>(mask)
          << ")";
    }
  }
}

TEST(IterationBoundaryImpossible,
     reachable_combinations_are_not_flagged_as_impossible) {
  // A "reachable" combination: every strict probe is supported by
  // its recoverable counterpart.
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    const auto facts = make_facts(mask);
    const bool firstViolates = facts.firstStrict && !facts.firstRecoverable;
    const bool followViolates = facts.followStrict && !facts.followRecoverable;
    if (!firstViolates && !followViolates) {
      EXPECT_FALSE(is_impossible_iteration_fact_combination(facts))
          << "reachable combination flagged as impossible (mask="
          << static_cast<int>(mask) << ")";
    }
  }
}

// -----------------------------------------------------------------------------
// 4. Integration scenarios
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, fresh_option_with_no_signal_does_not_synthesize) {
  // Fresh (not started), no first signal at all: the decision must
  // NOT be one of the synthesizing families (ContinueStrict /
  // ContinueRecoverable / RepairStartedIteration). It should be
  // Resync (last resort, if no follow blocks) or StopCleanly /
  // RejectToParentFollow if a follow is in scope.
  IterationBoundaryFacts facts;
  facts.startedStrictly = false;
  facts.firstStrict = false;
  facts.firstRecoverable = false;
  facts.followStrict = false;
  facts.followRecoverable = false;
  facts.committedPrefixImposes = false;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::Resync);
  // No first signal does NOT yield ContinueStrict or
  // ContinueRecoverable.
  EXPECT_NE(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
  EXPECT_NE(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueRecoverable);
}

TEST(IterationBoundaryPrecedence,
     follow_strict_wins_over_first_recoverable) {
  // When the parent strict follow accepts AND the element accepts
  // recoverable, the iteration must stop (or reject to parent),
  // never produce a recoverable repair.
  IterationBoundaryFacts facts;
  facts.firstStrict = false;
  facts.firstRecoverable = true;
  facts.followStrict = true;
  facts.followRecoverable = true;
  // Without committed: rule 2 (StopCleanly).
  facts.committedPrefixImposes = false;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::StopCleanly);
  // With committed: rule 6 (RejectToParentFollow).
  facts.committedPrefixImposes = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::RejectToParentFollow);
}

TEST(IterationBoundaryPrecedence,
     continue_strict_wins_over_stop_when_both_apply) {
  // Strict continuation wins explicitly over stop-on-strict-follow
  // when both apply: when firstStrict AND followStrict both hold,
  // ContinueStrict wins (rule 1 before rule 2).
  IterationBoundaryFacts facts;
  facts.firstStrict = true;
  facts.firstRecoverable = true;
  facts.followStrict = true;
  facts.followRecoverable = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
}

TEST(IterationBoundaryPrecedence,
     repair_started_iteration_requires_started) {
  // Delete retry never starts a hypothetical iteration:
  // RepairStartedIteration only fires when the iteration has
  // started. If startedStrictly=false, no RepairStartedIteration.
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    const auto facts = make_facts(mask);
    const auto decision = decide_iteration_boundary(facts);
    if (decision == IterationBoundaryDecision::RepairStartedIteration) {
      EXPECT_TRUE(facts.startedStrictly)
          << "RepairStartedIteration with startedStrictly=false (mask="
          << static_cast<int>(mask) << ")";
    }
  }
}

TEST(IterationBoundaryPrecedence, resync_only_when_no_other_rule_applies) {
  // Resync only wins as a last resort: it fires only when
  // firstStrict=F, followStrict=F, startedStrictly=F,
  // firstRecoverable=F. Any other applicable rule wins.
  for (std::uint8_t mask = 0; mask < 64U; ++mask) {
    const auto facts = make_facts(mask);
    if (decide_iteration_boundary(facts) ==
        IterationBoundaryDecision::Resync) {
      EXPECT_FALSE(facts.firstStrict);
      EXPECT_FALSE(facts.followStrict);
      EXPECT_FALSE(facts.startedStrictly);
      EXPECT_FALSE(facts.firstRecoverable);
    }
  }
}

// -----------------------------------------------------------------------------
// 5. Spot checks on individual rules to make the precedence visible in tests
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, rule_1_first_strict_yields_continue_strict) {
  IterationBoundaryFacts facts;
  facts.firstStrict = true;
  facts.firstRecoverable = true; // follows the impossibility relation
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
}

TEST(IterationBoundaryPrecedence,
     rule_2_follow_strict_without_committed_yields_stop_cleanly) {
  IterationBoundaryFacts facts;
  facts.followStrict = true;
  facts.followRecoverable = true;
  facts.committedPrefixImposes = false;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::StopCleanly);
}

TEST(IterationBoundaryPrecedence,
     rule_3_started_without_follow_strict_yields_repair) {
  IterationBoundaryFacts facts;
  facts.startedStrictly = true;
  // firstStrict=F (else rule 1 fires); followStrict=F (else blocks).
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::RepairStartedIteration);
}

TEST(IterationBoundaryPrecedence,
     rule_4_first_recoverable_without_follow_strict_yields_continue_recoverable) {
  IterationBoundaryFacts facts;
  facts.firstRecoverable = true;
  // startedStrictly=F (else rule 3 fires), firstStrict=F, followStrict=F
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueRecoverable);
}

TEST(IterationBoundaryPrecedence,
     rule_5_no_signals_without_follow_strict_yields_resync) {
  IterationBoundaryFacts facts; // all false
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::Resync);
}

TEST(IterationBoundaryPrecedence,
     rule_6_committed_with_follow_strict_yields_reject_to_parent) {
  IterationBoundaryFacts facts;
  facts.followStrict = true;
  facts.followRecoverable = true;
  facts.committedPrefixImposes = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::RejectToParentFollow);
}
