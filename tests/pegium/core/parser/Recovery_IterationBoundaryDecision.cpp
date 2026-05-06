/// `IterationBoundaryDecision` precedence table tests.
///
/// `Repetition` boundary precedence is a closed table on 4 boolean
/// facts. The table is total and exclusive: every one of the 16
/// combinations receives exactly one decision.
///
/// This suite covers:
///
///   1. Exhaustivity: all 16 combinations enumerated; each receives
///      a single decision.
///   2. Each rule of the closed precedence table fires for at least
///      one combination (no dead rule).
///   3. Five integration scenarios:
///        - a fresh option does not synthesise without an entry signal;
///        - strict parent follow wins;
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
using pegium::parser::detail::IterationBoundaryDecision;
using pegium::parser::detail::IterationBoundaryFacts;

namespace {

constexpr IterationBoundaryFacts make_facts(std::uint8_t mask) noexcept {
  return IterationBoundaryFacts{
      .startedStrictly = (mask & 0b0001U) != 0U,
      .firstStrict = (mask & 0b0010U) != 0U,
      .followStrict = (mask & 0b0100U) != 0U,
      .committedPrefixImposes = (mask & 0b1000U) != 0U,
  };
}

} // namespace

// -----------------------------------------------------------------------------
// 1. Exhaustivity: 16 combinations, each receives exactly one decision
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence,
     all_16_combinations_receive_exactly_one_decision) {
  std::array<int, 5> decisionCounts{};
  for (std::uint8_t mask = 0; mask < 16U; ++mask) {
    const auto facts = make_facts(mask);
    const auto decision = decide_iteration_boundary(facts);
    // Coverage assertion: the decision is one of the 5 closed values.
    switch (decision) {
    case IterationBoundaryDecision::StopCleanly:
    case IterationBoundaryDecision::ContinueStrict:
    case IterationBoundaryDecision::RepairStartedIteration:
    case IterationBoundaryDecision::Resync:
    case IterationBoundaryDecision::RejectToParentFollow:
      ++decisionCounts[static_cast<std::size_t>(decision)];
      break;
    }
  }
  // Sanity: the total counted equals 16 (no enum value escaped the
  // switch).
  int total = 0;
  for (int count : decisionCounts) {
    total += count;
  }
  EXPECT_EQ(total, 16);
}

// -----------------------------------------------------------------------------
// 2. Each rule fires at least once (no dead rule)
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, every_decision_value_is_reachable) {
  std::set<IterationBoundaryDecision> seen;
  for (std::uint8_t mask = 0; mask < 16U; ++mask) {
    seen.insert(decide_iteration_boundary(make_facts(mask)));
  }
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::StopCleanly));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::ContinueStrict));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::RepairStartedIteration));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::Resync));
  EXPECT_TRUE(seen.contains(IterationBoundaryDecision::RejectToParentFollow));
  EXPECT_EQ(seen.size(), 5U) << "unexpected new decision value";
}

// -----------------------------------------------------------------------------
// 3. Integration scenarios
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, fresh_option_with_no_signal_does_not_synthesize) {
  // Fresh (not started), no first signal at all: the decision must
  // NOT be one of the synthesizing families (ContinueStrict /
  // RepairStartedIteration). It should be Resync (last resort, if no
  // follow blocks) or StopCleanly / RejectToParentFollow if a follow
  // is in scope.
  IterationBoundaryFacts facts;
  facts.startedStrictly = false;
  facts.firstStrict = false;
  facts.followStrict = false;
  facts.committedPrefixImposes = false;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::Resync);
  // No first signal does NOT yield ContinueStrict.
  EXPECT_NE(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
}

TEST(IterationBoundaryPrecedence,
     follow_strict_yields_stop_or_reject) {
  // When the parent strict follow accepts, the iteration must stop
  // (or reject to parent), never produce a local repair.
  IterationBoundaryFacts facts;
  facts.firstStrict = false;
  facts.followStrict = true;
  // Without committed: rule 2 (StopCleanly).
  facts.committedPrefixImposes = false;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::StopCleanly);
  // With committed: rule 5 (RejectToParentFollow).
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
  facts.followStrict = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
}

TEST(IterationBoundaryPrecedence,
     repair_started_iteration_requires_started) {
  // Delete retry never starts a hypothetical iteration:
  // RepairStartedIteration only fires when the iteration has
  // started. If startedStrictly=false, no RepairStartedIteration.
  for (std::uint8_t mask = 0; mask < 16U; ++mask) {
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
  // firstStrict=F, followStrict=F, startedStrictly=F.
  // Any other applicable rule wins.
  for (std::uint8_t mask = 0; mask < 16U; ++mask) {
    const auto facts = make_facts(mask);
    if (decide_iteration_boundary(facts) ==
        IterationBoundaryDecision::Resync) {
      EXPECT_FALSE(facts.firstStrict);
      EXPECT_FALSE(facts.followStrict);
      EXPECT_FALSE(facts.startedStrictly);
    }
  }
}

// -----------------------------------------------------------------------------
// 4. Spot checks on individual rules to make the precedence visible in tests
// -----------------------------------------------------------------------------

TEST(IterationBoundaryPrecedence, rule_1_first_strict_yields_continue_strict) {
  IterationBoundaryFacts facts;
  facts.firstStrict = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::ContinueStrict);
}

TEST(IterationBoundaryPrecedence,
     rule_2_follow_strict_without_committed_yields_stop_cleanly) {
  IterationBoundaryFacts facts;
  facts.followStrict = true;
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
     rule_4_no_signals_without_follow_strict_yields_resync) {
  IterationBoundaryFacts facts; // all false
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::Resync);
}

TEST(IterationBoundaryPrecedence,
     rule_5_committed_with_follow_strict_yields_reject_to_parent) {
  IterationBoundaryFacts facts;
  facts.followStrict = true;
  facts.committedPrefixImposes = true;
  EXPECT_EQ(decide_iteration_boundary(facts),
            IterationBoundaryDecision::RejectToParentFollow);
}
