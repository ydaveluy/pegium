/// `ReplayPrefixClass` classification on `EditableRecoveryCandidate`.
///
/// `OrderedChoice` dominance reads the closed `ReplayPrefixClass`
/// axis of `RecoveryContract`. Each `EditableRecoveryCandidate` is
/// classified at construction via
/// `classify_editable_replay_prefix(editCount, hasDeleteEdit)`. The
/// dominance predicate reads the closed enum field instead of
/// re-deriving the family from raw edit counts and `hasDeleteEdit`
/// flags.
///
/// This suite pins the closed mapping the classifier implements:
///
///   - 0 edits                          -> `Empty`
///   - edits with at least one delete   -> `ExtendedCommittedPrefix`
///   - insert-only edits                -> `NewLocalPrefix`
///
/// `SameCommittedPrefix` is reserved for the `RecoverySearch` driver
/// that snapshots the committed prefix between attempts; the
/// per-attempt classifier in `OrderedChoice` does not see that
/// snapshot and therefore never produces this value. The tests below
/// verify both the mapping and the `SameCommittedPrefix` exclusion.
///
/// Cache neutrality and end-to-end behaviour of the dominance
/// predicate using this classification are exercised by the
/// cache-neutrality harness; this suite keeps to the unit-level
/// mapping contract.

#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryContract.hpp>

#include <gtest/gtest.h>

using pegium::parser::detail::classify_editable_replay_prefix;
using pegium::parser::detail::classify_structural_replay_prefix;
using pegium::parser::detail::EditableRecoveryCandidate;
using pegium::parser::detail::ReplayPrefixClass;
using pegium::parser::detail::StructuralProgressRecoveryCandidate;

TEST(ReplayPrefixClassification, zero_edits_classify_as_empty) {
  EXPECT_EQ(classify_editable_replay_prefix(/*editCount=*/0, false),
            ReplayPrefixClass::Empty);
  EXPECT_EQ(classify_editable_replay_prefix(/*editCount=*/0, true),
            ReplayPrefixClass::Empty);
}

TEST(ReplayPrefixClassification,
     non_zero_edits_with_delete_classify_as_extended_committed_prefix) {
  EXPECT_EQ(
      classify_editable_replay_prefix(/*editCount=*/1, /*hasDeleteEdit=*/true),
      ReplayPrefixClass::ExtendedCommittedPrefix);
  EXPECT_EQ(
      classify_editable_replay_prefix(/*editCount=*/5, /*hasDeleteEdit=*/true),
      ReplayPrefixClass::ExtendedCommittedPrefix);
}

TEST(ReplayPrefixClassification,
     insert_only_edits_classify_as_new_local_prefix) {
  EXPECT_EQ(
      classify_editable_replay_prefix(/*editCount=*/1, /*hasDeleteEdit=*/false),
      ReplayPrefixClass::NewLocalPrefix);
  EXPECT_EQ(
      classify_editable_replay_prefix(/*editCount=*/3, /*hasDeleteEdit=*/false),
      ReplayPrefixClass::NewLocalPrefix);
}

TEST(ReplayPrefixClassification,
     classifier_never_returns_same_committed_prefix) {
  // The per-attempt classifier reserves `SameCommittedPrefix` for
  // sites that compare the candidate against an already-committed
  // prefix snapshot. The `OrderedChoice` / `Group` / `Repetition`
  // construction sites do not have such a snapshot at hand and must
  // never produce this value through the classifier.
  for (std::uint32_t editCount = 0; editCount <= 4u; ++editCount) {
    for (const bool hasDeleteEdit : {false, true}) {
      const auto cls =
          classify_editable_replay_prefix(editCount, hasDeleteEdit);
      EXPECT_NE(cls, ReplayPrefixClass::SameCommittedPrefix)
          << "editCount=" << editCount
          << " hasDeleteEdit=" << hasDeleteEdit;
    }
  }
}

TEST(ReplayPrefixClassification, default_constructed_candidate_is_empty) {
  // The default value of `replayPrefix` on a freshly-constructed
  // candidate must be `Empty`, matching the classifier's mapping for
  // the zero-edit shape. This guarantees that placeholders (e.g.
  // `bestCandidate{}` accumulators) start at the safe class that
  // disqualifies them from dominance until they are filled.
  const EditableRecoveryCandidate candidate;
  EXPECT_EQ(candidate.replayPrefix, ReplayPrefixClass::Empty);
  EXPECT_EQ(candidate.editCount, 0u);
  EXPECT_FALSE(candidate.hasDeleteEdit);
}

// -----------------------------------------------------------------------------
// Structural classifier (Repetition's `IterationAttempt` candidate)
// -----------------------------------------------------------------------------

TEST(ReplayPrefixClassification,
     structural_no_edits_classify_as_empty) {
  EXPECT_EQ(classify_structural_replay_prefix(/*hadEdits=*/false,
                                               /*hasDestructiveEdit=*/false),
            ReplayPrefixClass::Empty);
  EXPECT_EQ(classify_structural_replay_prefix(/*hadEdits=*/false,
                                               /*hasDestructiveEdit=*/true),
            ReplayPrefixClass::Empty);
}

TEST(ReplayPrefixClassification,
     structural_destructive_edits_classify_as_extended_committed_prefix) {
  // The Repetition classifier treats Deleted OR Replaced as
  // destructive — both kinds commit to a strictly different replay
  // prefix.
  EXPECT_EQ(classify_structural_replay_prefix(/*hadEdits=*/true,
                                               /*hasDestructiveEdit=*/true),
            ReplayPrefixClass::ExtendedCommittedPrefix);
}

TEST(ReplayPrefixClassification,
     structural_insert_only_edits_classify_as_new_local_prefix) {
  EXPECT_EQ(classify_structural_replay_prefix(/*hadEdits=*/true,
                                               /*hasDestructiveEdit=*/false),
            ReplayPrefixClass::NewLocalPrefix);
}

TEST(ReplayPrefixClassification,
     default_constructed_structural_candidate_is_empty) {
  const StructuralProgressRecoveryCandidate candidate;
  EXPECT_EQ(candidate.replayPrefix, ReplayPrefixClass::Empty);
  EXPECT_FALSE(candidate.hadEdits);
  EXPECT_FALSE(candidate.hasDestructiveEdit);
}
