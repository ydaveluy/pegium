/// `GroupTransition` vocabulary and legality tests.
///
/// `Group` recovery is framed as a small machine of 5 transitions.
/// This suite covers the closed vocabulary and the legality
/// predicates that `Group.hpp` dispatch reads.
///
/// This suite covers:
///
///   1. The `GroupTransition` enum is closed at 5 documented values.
///   2. Each transition's legality predicate reads only its
///      documented facts.
///   3. The `is_group_transition_legal` dispatch is total over the
///      closed enum.
///   4. Per-transition spot checks for the canonical legal cases.

#include <pegium/core/parser/GroupTransition.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using pegium::parser::detail::GroupTransition;
using pegium::parser::detail::GroupTransitionLegalityFacts;
using pegium::parser::detail::group_transition_name;
using pegium::parser::detail::is_group_transition_legal;
using pegium::parser::detail::is_insert_missing_current_legal;
using pegium::parser::detail::is_keep_current_legal;
using pegium::parser::detail::is_repair_current_legal;
using pegium::parser::detail::is_repair_tail_legal;
using pegium::parser::detail::is_skip_nullable_legal;

// -----------------------------------------------------------------------------
// 1. Closed enums
// -----------------------------------------------------------------------------

TEST(GroupTransitionEnum, transition_values_are_documented_and_closed) {
  for (const auto value : {GroupTransition::KeepCurrent,
                            GroupTransition::RepairCurrent,
                            GroupTransition::SkipNullable,
                            GroupTransition::InsertMissingCurrent,
                            GroupTransition::RepairTail}) {
    switch (value) {
    case GroupTransition::KeepCurrent:
    case GroupTransition::RepairCurrent:
    case GroupTransition::SkipNullable:
    case GroupTransition::InsertMissingCurrent:
    case GroupTransition::RepairTail:
      SUCCEED();
      break;
    }
  }
}

TEST(GroupTransitionEnum, transition_names_are_stable) {
  EXPECT_STREQ(group_transition_name(GroupTransition::KeepCurrent),
               "KeepCurrent");
  EXPECT_STREQ(group_transition_name(GroupTransition::RepairCurrent),
               "RepairCurrent");
  EXPECT_STREQ(group_transition_name(GroupTransition::SkipNullable),
               "SkipNullable");
  EXPECT_STREQ(group_transition_name(GroupTransition::InsertMissingCurrent),
               "InsertMissingCurrent");
  EXPECT_STREQ(group_transition_name(GroupTransition::RepairTail),
               "RepairTail");
}

// -----------------------------------------------------------------------------
// 2. Facts struct shape
// -----------------------------------------------------------------------------

TEST(GroupTransitionFacts, default_facts_are_all_false) {
  const GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(facts.currentMatchesStrict);
  EXPECT_FALSE(facts.currentEntrySignal);
  EXPECT_FALSE(facts.currentNullable);
  EXPECT_FALSE(facts.currentVisibleLeafConsumed);
  EXPECT_FALSE(facts.currentStrictlyAcquired);
  EXPECT_FALSE(facts.parentFollowStrict);
  EXPECT_FALSE(facts.tailEntrySignalIndependent);
  EXPECT_FALSE(facts.tailNullable);
  EXPECT_FALSE(facts.insertionReplayable);
  EXPECT_FALSE(facts.previousNullableSiblingOwnsCursor);
}

TEST(GroupTransitionFacts, struct_is_trivially_copyable_and_small) {
  static_assert(std::is_trivially_copyable_v<GroupTransitionLegalityFacts>);
  EXPECT_LE(sizeof(GroupTransitionLegalityFacts), 16U);
}

// -----------------------------------------------------------------------------
// 3. Per-transition legality
// -----------------------------------------------------------------------------

TEST(GroupTransitionLegality, keep_current_requires_strict_match) {
  GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(is_keep_current_legal(facts));
  facts.currentMatchesStrict = true;
  EXPECT_TRUE(is_keep_current_legal(facts));
  // No other fact influences the decision.
  facts.parentFollowStrict = true;
  EXPECT_TRUE(is_keep_current_legal(facts));
  facts.currentNullable = true;
  EXPECT_TRUE(is_keep_current_legal(facts));
}

TEST(GroupTransitionLegality,
     repair_current_requires_entry_signal_and_no_strict_parent_follow) {
  GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(is_repair_current_legal(facts));
  // Entry signal alone is enough when no parent follow blocks.
  facts.currentEntrySignal = true;
  EXPECT_TRUE(is_repair_current_legal(facts));
  // Strict parent follow blocks.
  facts.parentFollowStrict = true;
  EXPECT_FALSE(is_repair_current_legal(facts));
  // No entry signal blocks regardless.
  facts.parentFollowStrict = false;
  facts.currentEntrySignal = false;
  EXPECT_FALSE(is_repair_current_legal(facts));
}

TEST(GroupTransitionLegality,
     skip_nullable_requires_nullable_and_no_visible_leaf_consumed) {
  GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(is_skip_nullable_legal(facts));
  // Nullable alone is enough when no visible leaf has been consumed.
  facts.currentNullable = true;
  EXPECT_TRUE(is_skip_nullable_legal(facts));
  // A consumed visible leaf disqualifies even a nullable element.
  facts.currentVisibleLeafConsumed = true;
  EXPECT_FALSE(is_skip_nullable_legal(facts));
  // Non-nullable element cannot be skipped even without consumption.
  facts.currentVisibleLeafConsumed = false;
  facts.currentNullable = false;
  EXPECT_FALSE(is_skip_nullable_legal(facts));
}

TEST(GroupTransitionLegality,
     insert_missing_current_requires_replayable_independent_tail) {
  GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
  // Required facts true and no sibling claims the cursor.
  facts.insertionReplayable = true;
  facts.tailEntrySignalIndependent = true;
  EXPECT_TRUE(is_insert_missing_current_legal(facts));
  // Visible leaf consumed disqualifies — current has started.
  facts.currentVisibleLeafConsumed = true;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
  facts.currentVisibleLeafConsumed = false;
  // Insertion not replayable disqualifies.
  facts.insertionReplayable = false;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
  facts.insertionReplayable = true;
  // Tail entry signal not independent disqualifies (it would be a
  // self-justifying insertion).
  facts.tailEntrySignalIndependent = false;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
}

TEST(GroupTransitionLegality,
     insert_missing_current_rejects_previous_nullable_sibling_owning_cursor) {
  GroupTransitionLegalityFacts facts;
  facts.insertionReplayable = true;
  facts.tailEntrySignalIndependent = true;
  EXPECT_TRUE(is_insert_missing_current_legal(facts));
  facts.previousNullableSiblingOwnsCursor = true;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
}

TEST(GroupTransitionLegality,
     repair_tail_requires_current_strictly_acquired) {
  GroupTransitionLegalityFacts facts;
  EXPECT_FALSE(is_repair_tail_legal(facts));
  // Strictly acquired current (cursor advanced or edits accumulated)
  // allows tail repair.
  facts.currentStrictlyAcquired = true;
  EXPECT_TRUE(is_repair_tail_legal(facts));
  // A nullable current that did NOT commit progress does NOT admit
  // RepairTail: that path would duplicate `SkipNullable` (already
  // attempted earlier) at the same cursor with no new information.
  facts.currentStrictlyAcquired = false;
  facts.currentNullable = true;
  EXPECT_FALSE(is_repair_tail_legal(facts));
  // Nullable + acquired (e.g. nullable wrapper around a successful
  // match that consumed cursor) is admissible via the strict axis.
  facts.currentStrictlyAcquired = true;
  EXPECT_TRUE(is_repair_tail_legal(facts));
  // Neither: not allowed.
  facts.currentStrictlyAcquired = false;
  facts.currentNullable = false;
  EXPECT_FALSE(is_repair_tail_legal(facts));
}

// -----------------------------------------------------------------------------
// 4. Dispatch is total over the closed enum
// -----------------------------------------------------------------------------

TEST(GroupTransitionLegality, dispatch_calls_each_predicate) {
  GroupTransitionLegalityFacts allTrue;
  allTrue.currentMatchesStrict = true;
  allTrue.currentEntrySignal = true;
  allTrue.currentNullable = true;
  allTrue.currentVisibleLeafConsumed = false;
  allTrue.currentStrictlyAcquired = true;
  allTrue.parentFollowStrict = false;
  allTrue.tailEntrySignalIndependent = true;
  allTrue.tailNullable = true;
  allTrue.insertionReplayable = true;
  allTrue.previousNullableSiblingOwnsCursor = false;
  // Every transition should be legal under "all-positive" facts.
  EXPECT_TRUE(is_group_transition_legal(GroupTransition::KeepCurrent, allTrue));
  EXPECT_TRUE(is_group_transition_legal(GroupTransition::RepairCurrent, allTrue));
  EXPECT_TRUE(is_group_transition_legal(GroupTransition::SkipNullable, allTrue));
  EXPECT_TRUE(
      is_group_transition_legal(GroupTransition::InsertMissingCurrent, allTrue));
  EXPECT_TRUE(is_group_transition_legal(GroupTransition::RepairTail, allTrue));
}

TEST(GroupTransitionLegality, dispatch_default_facts_block_all_transitions) {
  const GroupTransitionLegalityFacts allFalse;
  EXPECT_FALSE(
      is_group_transition_legal(GroupTransition::KeepCurrent, allFalse));
  EXPECT_FALSE(
      is_group_transition_legal(GroupTransition::RepairCurrent, allFalse));
  EXPECT_FALSE(
      is_group_transition_legal(GroupTransition::SkipNullable, allFalse));
  EXPECT_FALSE(is_group_transition_legal(
      GroupTransition::InsertMissingCurrent, allFalse));
  EXPECT_FALSE(
      is_group_transition_legal(GroupTransition::RepairTail, allFalse));
}

// -----------------------------------------------------------------------------
// 5. RepairTail does not justify Current's entry
// -----------------------------------------------------------------------------

TEST(GroupTransitionLegality,
     repair_tail_does_not_justify_current_entry_via_currentEntrySignal) {
  // RepairTail never retroactively justifies Current's entry. The
  // legality predicate verifies this by not consulting
  // `currentEntrySignal` at all — `RepairTail`'s legality reads only
  // `currentStrictlyAcquired` and `currentNullable`.
  GroupTransitionLegalityFacts facts;
  facts.currentEntrySignal = true; // entry signal exists
  facts.currentStrictlyAcquired = false;
  facts.currentNullable = false;
  // RepairTail must reject because current is neither acquired nor
  // nullable, regardless of the entry signal.
  EXPECT_FALSE(is_repair_tail_legal(facts));
}

TEST(GroupTransitionLegality,
     insert_missing_current_does_not_self_justify_via_inserted_element) {
  // `InsertMissingCurrent` requires the tail to provide a
  // continuation independent of the inserted element, encoded as
  // `tailEntrySignalIndependent = true`. If false, the insertion
  // cannot self-justify even with all other facts true.
  GroupTransitionLegalityFacts facts;
  facts.insertionReplayable = true;
  facts.tailEntrySignalIndependent = false;
  EXPECT_FALSE(is_insert_missing_current_legal(facts));
}
