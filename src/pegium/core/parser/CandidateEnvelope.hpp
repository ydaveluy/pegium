#pragma once

/// `CandidateEnvelope`: the recovery candidate viewed through the
/// shared admission/dominance/ranking pipeline.
///
/// The envelope is the small common type that admission, dominance,
/// and ranking all read. It is intentionally minimal:
///
///   - `key`        — the existing `RecoveryKey` ranking tuple. The
///                    final preference order remains lexicographic on
///                    this key.
///   - `contract`   — what the candidate observes about its position;
///                    closed enums, no preference.
///   - `evidence`   — explanatory residue: facts observed, named
///                    reasons, offsets, costs. NEVER consulted by
///                    decision rules; this is non-negotiable.
///   - `origin`     — where the candidate came from; one closed enum
///                    value per producing site.
///
/// `CandidateOrigin` is where the candidate came from, not why it is
/// admissible. It does not participate in ranking.
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <cstdint>
#include <optional>
#include <type_traits>

#include <pegium/core/parser/RecoveryCandidate.hpp>
#include <pegium/core/parser/RecoveryContract.hpp>
#include <pegium/core/utils/TextUtils.hpp>

namespace pegium::parser::detail {

/// Where the candidate was produced. Read by the family-redundancy
/// predicates which require `next.origin == current.origin` to forbid
/// cross-combinator dominance comparisons.
enum class CandidateOrigin : std::uint8_t {
  /// Unset / candidate not yet attributed to a producer. Admission
  /// rejects unattributed candidates.
  Unspecified,
  /// Produced by an `OrderedChoice` branch.
  OrderedChoiceBranch,
  /// Produced by a `Repetition` iteration.
  RepetitionIteration,
};

/// The candidate envelope. Three axes:
///   - `key`: ranking. Read by `is_better_recovery_key`.
///   - `contract`: closed-enum structural observations. Read by the
///     family-redundancy predicates.
///   - `origin`: where the candidate was produced. Used by the
///     family-redundancy predicates to forbid cross-combinator
///     comparisons (`next.origin == current.origin`).
struct CandidateEnvelope {
  RecoveryKey key{};
  RecoveryContract contract;
  CandidateOrigin origin = CandidateOrigin::Unspecified;
};

static_assert(std::is_trivially_copyable_v<CandidateEnvelope>,
              "CandidateEnvelope must remain trivially copyable so it can be "
              "moved cheaply through the pipeline (admission, dominance, "
              "ranking) without allocations.");

/// Maps an existing `EditableRecoveryCandidate` to the envelope.
[[nodiscard]] inline CandidateEnvelope
to_candidate_envelope(const EditableRecoveryCandidate &candidate,
                      CandidateOrigin origin =
                          CandidateOrigin::Unspecified) noexcept {
  return {
      .key = editable_recovery_key(candidate),
      .contract =
          {
              .continuation = candidate.editCount != 0u &&
                                      continues_after_first_edit(candidate)
                                  ? ContinuationRequirement::
                                        VisibleContinuationAfterEdit
                                  : ContinuationRequirement::None,
              .replayPrefix = candidate.replayPrefix,
          },
      .origin = origin,
  };
}

/// Maps an existing `StructuralProgressRecoveryCandidate` (the
/// `Repetition` candidate type) to the envelope.
[[nodiscard]] inline CandidateEnvelope
to_candidate_envelope(const StructuralProgressRecoveryCandidate &candidate,
                      CandidateOrigin origin =
                          CandidateOrigin::RepetitionIteration) noexcept {
  return {
      .key = structural_progress_recovery_key(candidate),
      .contract =
          {
              .continuation =
                  candidate.hadEdits && candidate.continuesAfterFirstEdit
                      ? ContinuationRequirement::VisibleContinuationAfterEdit
                      : ContinuationRequirement::None,
              .replayPrefix = candidate.replayPrefix,
          },
      .origin = origin,
  };
}

// -----------------------------------------------------------------------------
// Family-redundancy predicates (free functions)
// -----------------------------------------------------------------------------
//
// Dominance must not cross parent/child: these predicates compare two
// candidates within a single combinator's enumeration. A candidate
// produced by a parent scope must never be filtered by a candidate
// produced by a child scope (and vice-versa). The predicates encode
// this rule by rejecting any pair whose `CandidateOrigin` values
// differ — candidates from different combinators (or from explicitly
// non-comparable origins) cannot dominate each other through these
// per-anchor redundancy filters.
//
// The predicates are NOT replay-equivalence dominance: the two
// candidates carry different scripts. They are structural preferences
// inside a same-anchor extension family, applied as redundancy
// filters before the central `RecoveryKey` ranking sees the pair.

/// `extension_outranks_anchor_base`: `next` outranks `current` when
/// `next` carries `ReplayPrefixClass::ExtendedCommittedPrefix` over
/// the same anchor (`firstEditOffset`) at strictly higher cost AND
/// strictly further progress, with `current` carrying any non-empty
/// committed prefix at the same anchor. Used by `OrderedChoice` to
/// remove redundant base attempts when an extending attempt is
/// available at the same anchor.
[[nodiscard]] inline constexpr bool
extension_outranks_anchor_base(const CandidateEnvelope &next,
                                const CandidateEnvelope &current) noexcept {
  // Parent/child guard: candidates from different origins are never
  // comparable through this filter.
  if (next.origin != current.origin) {
    return false;
  }
  if (!(next.key.matched && current.key.matched &&
        next.contract.replayPrefix ==
            ReplayPrefixClass::ExtendedCommittedPrefix &&
        current.contract.replayPrefix != ReplayPrefixClass::Empty &&
        next.key.firstEditOffset == current.key.firstEditOffset &&
        next.key.editCost > current.key.editCost &&
        next.key.progressAfterEdits > current.key.progressAfterEdits)) {
    return false;
  }
  // Cost-vs-progress ratio guard: when `current` already made real
  // forward progress past its edit position (i.e. its edits actually
  // consumed input rather than just synthesizing a node in place),
  // require each extra unit of edit cost in `next` to buy at least
  // one extra unit of progress before declaring it the legitimate
  // extension.
  //
  // Without this guard, a costly delete-cascade restart from one
  // OrderedChoice branch dominates a sibling branch's cheap repair
  // (insert-with-real-consumption like `datatype <ID>`, or fuzzy
  // keyword replace) just because the cascade reaches further by
  // skipping more input. That is wrong: the cheap branch already
  // consumed efficiently, the expensive cascade only "extends" by
  // burning budget on input the cheap branch would have left for
  // an outer recovery.
  //
  // The "real forward progress" check (`progressAfterEdits >
  // firstEditOffset`) is what distinguishes a genuine repair (e.g.
  // fuzzy `entit -> entity` consuming the keyword span, or
  // `datatype <ID>` inserted then ID consumed) from an insert-only
  // synthesis that fabricates a node without advancing the cursor
  // (e.g. inserting all elements of a Feature without consuming any
  // input). The latter legitimately loses to a destructive extension
  // that does skip past garbage; the former should not.
  if (current.key.progressAfterEdits > current.key.firstEditOffset) {
    // A genuine extension adds a small number of edits to consume a
    // few more chars. The cost gap should fit within a small budget
    // — at most one full Delete (cost 4) — for a single legitimate
    // extending delete to dominate. Beyond that, the "extension" is
    // really an alternative recovery strategy with a much worse
    // cost-to-progress ratio that should not silently displace a
    // candidate that already consumed input efficiently.
    constexpr std::uint32_t kMaxExtensionCostGap = 4u;
    const auto costGap = next.key.editCost - current.key.editCost;
    if (costGap > kMaxExtensionCostGap) {
      return false;
    }
    const auto progressGap =
        next.key.progressAfterEdits - current.key.progressAfterEdits;
    return progressGap >= costGap;
  }
  return true;
}

/// `boundary_repair_outranks_no_edit_iteration`: `candidate`
/// outranks `committed` when `candidate` carries a non-empty
/// replay prefix anchored at the same `firstEditOffset` as the
/// no-edit `committed`, progresses strictly further, and continues
/// after the edit (`VisibleContinuationAfterEdit`). Used by
/// `Repetition` to prefer a boundary repair over a trivial
/// no-edit iteration when both target the same boundary.
[[nodiscard]] inline constexpr bool
boundary_repair_outranks_no_edit_iteration(
    const CandidateEnvelope &candidate,
    const CandidateEnvelope &committed) noexcept {
  if (candidate.origin != committed.origin) {
    return false;
  }
  return candidate.key.matched && committed.key.matched &&
         candidate.contract.replayPrefix != ReplayPrefixClass::Empty &&
         committed.contract.replayPrefix == ReplayPrefixClass::Empty &&
         candidate.key.firstEditOffset == committed.key.firstEditOffset &&
         candidate.key.progressAfterEdits >
             committed.key.progressAfterEdits &&
         candidate.contract.continuation ==
             ContinuationRequirement::VisibleContinuationAfterEdit;
}

/// `destructive_extension_outranks_anchor_base`: `next` outranks
/// `current` when both attempt destructive edits anchored at the
/// same `firstEditOffset`, `next` carries
/// `ReplayPrefixClass::ExtendedCommittedPrefix` (its replay
/// prefix carries destructive edits — Deleted or Replaced — that
/// strictly extend the committed-prefix family), `current` shares
/// a non-empty prefix at the same anchor, and `next` progresses
/// strictly further at strictly higher cost while continuing
/// after the edit. Mirror of `extension_outranks_anchor_base`
/// for the destructive-edit family used by `Repetition`.
[[nodiscard]] inline constexpr bool
destructive_extension_outranks_anchor_base(
    const CandidateEnvelope &next,
    const CandidateEnvelope &current) noexcept {
  if (next.origin != current.origin) {
    return false;
  }
  if (!(next.key.matched && current.key.matched &&
        next.contract.replayPrefix ==
            ReplayPrefixClass::ExtendedCommittedPrefix &&
        current.contract.replayPrefix != ReplayPrefixClass::Empty &&
        next.key.firstEditOffset == current.key.firstEditOffset &&
        next.key.editCost > current.key.editCost &&
        next.key.progressAfterEdits > current.key.progressAfterEdits &&
        next.contract.continuation ==
            ContinuationRequirement::VisibleContinuationAfterEdit)) {
    return false;
  }
  // The "extension" rationale fires uniformly for any pair where `next`
  // costs more and progresses further at the same anchor. That is
  // correct when `current` is an insert-only synthesis that did not
  // consume any input (a 3-insert candidate that fabricates a
  // syntactic shape without advancing the cursor — a costly delete
  // that actually skips past garbage IS the better recovery there).
  //
  // It is incorrect when `current` is itself a destructive recovery
  // that already consumed input cheaply (e.g. a 1-cost fuzzy replace
  // of a typoed keyword): a 32-cost delete-cascade that "extends" by
  // skipping the whole subsequent line is not a genuine extension of
  // the fuzzy repair, just an alternative strategy with a much worse
  // cost-to-progress ratio. Requiring `progressGap >= costGap` in the
  // destructive-vs-destructive case keeps the original behavior for
  // insert-only `current` candidates (the resync delete legitimately
  // dominates them) while rejecting "skip everything" candidates
  // dressed up as extensions of a cheap fuzzy repair.
  if (current.contract.replayPrefix ==
      ReplayPrefixClass::ExtendedCommittedPrefix) {
    const auto costGap = next.key.editCost - current.key.editCost;
    const auto progressGap =
        next.key.progressAfterEdits - current.key.progressAfterEdits;
    return progressGap >= costGap;
  }
  return true;
}

} // namespace pegium::parser::detail
