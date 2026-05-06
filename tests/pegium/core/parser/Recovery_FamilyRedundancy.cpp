/// Family-redundancy filter parent/child invariant.
///
/// Dominance must not cross parent/child: a parent candidate must
/// never be removed from the DominanceSet by an unwrapped child
/// candidate, and vice-versa.
///
/// The three family-redundancy filters
/// (`extension_outranks_anchor_base`,
/// `boundary_repair_outranks_no_edit_iteration`,
/// `destructive_extension_outranks_anchor_base`) live as free
/// functions in `CandidateEnvelope.hpp`. Each carries an `origin`
/// guard: when the two envelopes carry different `CandidateOrigin`
/// values, the predicate returns `false` regardless of how the rest
/// of the facts would resolve. The combinators (`OrderedChoice`,
/// `Repetition`) delegate to these free functions, so the
/// parent/child invariant is enforced uniformly through one code
/// path.
///
/// This suite covers:
///
///   1. Each predicate returns true for canonical same-origin
///      pairs that satisfy all positive conditions (positive
///      control).
///   2. Each predicate returns false when the two envelopes carry
///      different origins, even when every other fact would say
///      true (the parent/child guard).
///   3. The guard is symmetric: swapping (next, current) still
///      rejects when origins differ.

#include <pegium/core/parser/CandidateEnvelope.hpp>
#include <pegium/core/parser/RecoveryContract.hpp>

#include <gtest/gtest.h>

using pegium::parser::detail::boundary_repair_outranks_no_edit_iteration;
using pegium::parser::detail::CandidateEnvelope;
using pegium::parser::detail::CandidateOrigin;
using pegium::parser::detail::destructive_extension_outranks_anchor_base;
using pegium::parser::detail::extension_outranks_anchor_base;
using pegium::parser::detail::ReplayPrefixClass;

namespace {

// -----------------------------------------------------------------------------
// Builders for canonical "outranks" pairs
// -----------------------------------------------------------------------------

/// Build a `(next, current)` pair that satisfies every positive
/// condition of `extension_outranks_anchor_base` except origin.
/// Caller assigns origins.
[[nodiscard]] std::pair<CandidateEnvelope, CandidateEnvelope>
make_extension_outranks_pair() noexcept {
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 5;
  next.key.editCost = 3;
  next.key.progressAfterEdits = 10;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 5;
  current.key.editCost = 1;
  current.key.progressAfterEdits = 7;
  current.contract.replayPrefix = ReplayPrefixClass::SameCommittedPrefix;
  return {next, current};
}

/// Build a `(candidate, committed)` pair that satisfies every
/// positive condition of `boundary_repair_outranks_no_edit_iteration`
/// except origin.
[[nodiscard]] std::pair<CandidateEnvelope, CandidateEnvelope>
make_boundary_repair_outranks_pair() noexcept {
  CandidateEnvelope candidate;
  candidate.key.matched = true;
  candidate.key.firstEditOffset = 5;
  candidate.key.editCost = 2;
  candidate.key.progressAfterEdits = 12;
  candidate.contract.replayPrefix = ReplayPrefixClass::SameCommittedPrefix;
  CandidateEnvelope committed;
  committed.key.matched = true;
  committed.key.firstEditOffset = 5;
  committed.key.editCost = 0;
  committed.key.progressAfterEdits = 6;
  committed.contract.replayPrefix = ReplayPrefixClass::Empty;
  return {candidate, committed};
}

/// Build a `(next, current)` pair that satisfies every positive
/// condition of `destructive_extension_outranks_anchor_base`
/// except origin.
[[nodiscard]] std::pair<CandidateEnvelope, CandidateEnvelope>
make_destructive_extension_outranks_pair() noexcept {
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 8;
  next.key.editCost = 4;
  next.key.progressAfterEdits = 15;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 8;
  current.key.editCost = 2;
  current.key.progressAfterEdits = 10;
  current.contract.replayPrefix = ReplayPrefixClass::SameCommittedPrefix;
  return {next, current};
}

} // namespace

// -----------------------------------------------------------------------------
// 1. Positive control: same-origin pairs that satisfy all conditions
// -----------------------------------------------------------------------------

TEST(FamilyRedundancyParentChild,
     extension_outranks_returns_true_for_same_origin_canonical_pair) {
  auto [next, current] = make_extension_outranks_pair();
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_TRUE(extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyParentChild,
     boundary_repair_returns_true_for_same_origin_canonical_pair) {
  auto [candidate, committed] = make_boundary_repair_outranks_pair();
  candidate.origin = CandidateOrigin::RepetitionIteration;
  committed.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_TRUE(boundary_repair_outranks_no_edit_iteration(candidate, committed));
}

TEST(FamilyRedundancyParentChild,
     destructive_extension_returns_true_for_same_origin_canonical_pair) {
  auto [next, current] = make_destructive_extension_outranks_pair();
  next.origin = CandidateOrigin::RepetitionIteration;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_TRUE(destructive_extension_outranks_anchor_base(next, current));
}

// -----------------------------------------------------------------------------
// 2. Parent/child guard: cross-origin pairs MUST return false
// -----------------------------------------------------------------------------

TEST(FamilyRedundancyParentChild,
     extension_outranks_rejects_cross_origin_pairs) {
  auto [next, current] = make_extension_outranks_pair();
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_FALSE(extension_outranks_anchor_base(next, current));
  // Swap roles: still rejects (guard is symmetric).
  next.origin = CandidateOrigin::RepetitionIteration;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_FALSE(extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyParentChild,
     boundary_repair_rejects_cross_origin_pairs) {
  auto [candidate, committed] = make_boundary_repair_outranks_pair();
  candidate.origin = CandidateOrigin::RepetitionIteration;
  committed.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_FALSE(
      boundary_repair_outranks_no_edit_iteration(candidate, committed));
  candidate.origin = CandidateOrigin::OrderedChoiceBranch;
  committed.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_FALSE(
      boundary_repair_outranks_no_edit_iteration(candidate, committed));
}

TEST(FamilyRedundancyParentChild,
     destructive_extension_rejects_cross_origin_pairs) {
  auto [next, current] = make_destructive_extension_outranks_pair();
  next.origin = CandidateOrigin::RepetitionIteration;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_FALSE(destructive_extension_outranks_anchor_base(next, current));
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_FALSE(destructive_extension_outranks_anchor_base(next, current));
}

// -----------------------------------------------------------------------------
// 3. Unspecified origin: rejects against every named origin
// -----------------------------------------------------------------------------

TEST(FamilyRedundancyParentChild,
     extension_outranks_rejects_unspecified_origin_against_any_origin) {
  // Unspecified vs named: the guard rejects (origin equality is
  // strict). This is the right behaviour because Unspecified is the
  // "candidate not yet attributed" sentinel — nothing is supposed
  // to compare against it.
  auto [next, current] = make_extension_outranks_pair();
  next.origin = CandidateOrigin::Unspecified;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_FALSE(extension_outranks_anchor_base(next, current));
}

// -----------------------------------------------------------------------------
// 4. Pair-wise origin matrix: every named origin pairs with itself,
//    and only with itself, succeeds (when all other conditions hold)
// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------
// 4. Cost-vs-progress ratio guard
//
// `extension_outranks_anchor_base` and
// `destructive_extension_outranks_anchor_base` both gate the
// "extension" promotion behind a cost-vs-progress ratio: each extra
// unit of edit cost must buy at least one extra unit of progress
// before `next` is allowed to dominate `current` as an extension.
// Without this guard, a costly delete-cascade restart inflates a
// branch's "progress" by skipping past garbage, falsely dominating
// a sibling branch's cheap fuzzy-keyword repair (or any other
// candidate that already consumed input efficiently).
// -----------------------------------------------------------------------------
TEST(FamilyRedundancyRatioGuard,
     extension_outranks_rejects_costly_extension_over_efficient_current) {
  // Canonical "two consecutive typos with clean trailing" pattern:
  // current is a cheap fuzzy keyword replace at offset 0 that
  // consumed `entit` + body and ended at offset 11 for cost 1. next
  // is a costly delete-cascade restart at the same anchor that
  // skipped to offset 23 for cost 32. The cascade's extra 12 units
  // of progress do NOT justify its extra 31 units of cost; the
  // cheap repair must win.
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 0;
  next.key.editCost = 32;
  next.key.progressAfterEdits = 23;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 0;
  current.key.editCost = 1;
  current.key.progressAfterEdits = 11;
  current.contract.replayPrefix = ReplayPrefixClass::NewLocalPrefix;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_FALSE(extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyRatioGuard,
     extension_outranks_accepts_progress_justifies_extra_cost) {
  // When the extra cost actually buys at least equivalent extra
  // progress, the extension promotion holds. Mirrors a legitimate
  // case where one branch did 1 extra delete (cost 4) to consume 4
  // more chars compared to a sibling branch.
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 0;
  next.key.editCost = 5;
  next.key.progressAfterEdits = 14;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 0;
  current.key.editCost = 1;
  current.key.progressAfterEdits = 10;
  current.contract.replayPrefix = ReplayPrefixClass::NewLocalPrefix;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_TRUE(extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyRatioGuard,
     extension_outranks_does_not_apply_ratio_to_insert_only_no_progress_current) {
  // When `current` is an insert-only synthesis that did NOT consume
  // input past its edit (progressAfterEdits == firstEditOffset), the
  // ratio guard does NOT apply: a destructive extension that
  // actually skips past garbage IS the better recovery there.
  // Mirrors `entity Blog { <<<<<...  title: String  }` where
  // "delete the garbage line" legitimately dominates a synthetic
  // Feature insertion that did not consume any character.
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 16;
  next.key.editCost = 100;
  next.key.progressAfterEdits = 50;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::OrderedChoiceBranch;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 16;
  current.key.editCost = 3;
  current.key.progressAfterEdits = 16; // no advance past edit position
  current.contract.replayPrefix = ReplayPrefixClass::NewLocalPrefix;
  current.origin = CandidateOrigin::OrderedChoiceBranch;
  EXPECT_TRUE(extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyRatioGuard,
     destructive_extension_rejects_costly_extension_over_cheap_destructive_current) {
  // Repetition-side variant: both `next` and `current` are
  // destructive, but `next` (a delete-cascade) costs much more for
  // marginal extra progress than `current` (a cheap fuzzy replace).
  // The ratio guard rejects the promotion.
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 34;
  next.key.editCost = 110;
  next.key.progressAfterEdits = 126;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::RepetitionIteration;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 34;
  current.key.editCost = 1;
  current.key.progressAfterEdits = 71;
  current.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_FALSE(destructive_extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyRatioGuard,
     destructive_extension_accepts_when_ratio_is_balanced) {
  // Cost gap 4, progress gap 4 (each extra delete consumes 4 chars).
  // The ratio is balanced; the extension promotion holds.
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 10;
  next.key.editCost = 8;
  next.key.progressAfterEdits = 18;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::RepetitionIteration;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 10;
  current.key.editCost = 4;
  current.key.progressAfterEdits = 14;
  current.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_TRUE(destructive_extension_outranks_anchor_base(next, current));
}

TEST(FamilyRedundancyRatioGuard,
     destructive_extension_does_not_apply_ratio_to_non_destructive_current) {
  // The Repetition-side guard only fires when BOTH candidates
  // already commit destructive edits. When `current` carries an
  // insert-only prefix (NewLocalPrefix), the ratio gate does not
  // apply and the destructive extension is allowed to dominate
  // (mirrors the OrderedChoice "no-progress insert" carve-out).
  CandidateEnvelope next;
  next.key.matched = true;
  next.key.firstEditOffset = 0;
  next.key.editCost = 50;
  next.key.progressAfterEdits = 60;
  next.contract.replayPrefix =
      ReplayPrefixClass::ExtendedCommittedPrefix;
  next.origin = CandidateOrigin::RepetitionIteration;
  CandidateEnvelope current;
  current.key.matched = true;
  current.key.firstEditOffset = 0;
  current.key.editCost = 2;
  current.key.progressAfterEdits = 20;
  current.contract.replayPrefix = ReplayPrefixClass::NewLocalPrefix;
  current.origin = CandidateOrigin::RepetitionIteration;
  EXPECT_TRUE(destructive_extension_outranks_anchor_base(next, current));
}

// -----------------------------------------------------------------------------
// 5. Pair-wise origin matrix: every named origin pairs with itself,
//    and only with itself, succeeds (when all other conditions hold)
// -----------------------------------------------------------------------------

TEST(FamilyRedundancyParentChild,
     extension_outranks_origin_matrix_is_diagonal) {
  // Sweep every distinct origin pair: only the diagonal succeeds.
  // Pins the parent/child invariant exhaustively over the closed
  // `CandidateOrigin` enum.
  static constexpr CandidateOrigin kOrigins[] = {
      CandidateOrigin::Unspecified,
      CandidateOrigin::OrderedChoiceBranch,
      CandidateOrigin::RepetitionIteration,
  };
  // `Unspecified` is the sentinel for unattributed candidates; its
  // diagonal is also expected to reject because the existing
  // admission rules already reject unattributed candidates upstream.
  // We accept it here as a "diagonal-but-rejected" entry.
  for (const auto a : kOrigins) {
    for (const auto b : kOrigins) {
      auto [next, current] = make_extension_outranks_pair();
      next.origin = a;
      current.origin = b;
      const bool actual = extension_outranks_anchor_base(next, current);
      const bool expectedTrue = (a == b);
      if (expectedTrue) {
        EXPECT_TRUE(actual)
            << "origin a=" << static_cast<int>(a)
            << " b=" << static_cast<int>(b);
      } else {
        EXPECT_FALSE(actual)
            << "origin a=" << static_cast<int>(a)
            << " b=" << static_cast<int>(b);
      }
    }
  }
}
