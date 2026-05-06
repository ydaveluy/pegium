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

#include <pegium/core/parser/ContextShared.hpp>
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
[[nodiscard]] CandidateEnvelope to_candidate_envelope(
    const EditableRecoveryCandidate &candidate,
    CandidateOrigin origin = CandidateOrigin::Unspecified) noexcept;

/// Maps an existing `StructuralProgressRecoveryCandidate` (the
/// `Repetition` candidate type) to the envelope.
[[nodiscard]] CandidateEnvelope to_candidate_envelope(
    const StructuralProgressRecoveryCandidate &candidate,
    CandidateOrigin origin = CandidateOrigin::RepetitionIteration) noexcept;

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

/// Shared admission shape for the extension-dominance predicates.
/// True iff both candidates share the same origin and anchor
/// (`firstEditOffset`), `next` carries
/// `ReplayPrefixClass::ExtendedCommittedPrefix`, `current` carries
/// any non-empty replay prefix, `next` strictly progresses further
/// at no-worse cost. Per-predicate ratio/continuation guards are
/// layered on top of this baseline.
[[nodiscard]] bool
is_extending_anchor_pair(const CandidateEnvelope &next,
                         const CandidateEnvelope &current) noexcept;

/// `extension_outranks_anchor_base`: `next` outranks `current` when
/// `next` carries `ReplayPrefixClass::ExtendedCommittedPrefix` over
/// the same anchor (`firstEditOffset`) at strictly higher cost AND
/// strictly further progress, with `current` carrying any non-empty
/// committed prefix at the same anchor. Used by `OrderedChoice` to
/// remove redundant base attempts when an extending attempt is
/// available at the same anchor.
[[nodiscard]] bool
extension_outranks_anchor_base(const CandidateEnvelope &next,
                               const CandidateEnvelope &current) noexcept;

/// `boundary_repair_outranks_no_edit_iteration`: `candidate`
/// outranks `committed` when `candidate` carries a non-empty
/// replay prefix, `committed` carries none, and `candidate`
/// progresses strictly further. Used by `Repetition` to prefer a
/// boundary repair over a trivial no-edit iteration.
[[nodiscard]] bool boundary_repair_outranks_no_edit_iteration(
    const CandidateEnvelope &candidate,
    const CandidateEnvelope &committed) noexcept;

/// `destructive_extension_outranks_anchor_base`: `next` outranks
/// `current` when both attempt destructive edits anchored at the
/// same `firstEditOffset`, `next` carries
/// `ReplayPrefixClass::ExtendedCommittedPrefix` (its replay
/// prefix carries destructive edits — Deleted or Replaced — that
/// strictly extend the committed-prefix family), `current` shares
/// a non-empty prefix at the same anchor, and `next` progresses
/// strictly further at strictly higher cost. Mirror of
/// `extension_outranks_anchor_base` for the destructive-edit family
/// used by `Repetition`.
[[nodiscard]] bool destructive_extension_outranks_anchor_base(
    const CandidateEnvelope &next,
    const CandidateEnvelope &current) noexcept;

} // namespace pegium::parser::detail
