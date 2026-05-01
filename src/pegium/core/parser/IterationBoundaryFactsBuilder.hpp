#pragma once

/// `IterationBoundaryFactsBuilder`: maps the recovery context state
/// to the closed `IterationBoundaryFacts` consumed by
/// `decide_iteration_boundary`.
///
/// The helper is a pure projection of context fields onto the 6-fact
/// bundle. It does not mutate the context. It does not run a parse;
/// it only reads facts the caller has already observed. Probe results
/// (first strict / first recoverable) are passed in by the caller
/// because they are combinator-specific (the element type, the
/// observation harness) and the projection helper stays element-
/// agnostic.
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <pegium/core/parser/IterationBoundaryDecision.hpp>
#include <pegium/core/parser/ParseContext.hpp>

namespace pegium::parser::detail {

/// Captures the two element-level probe facts the
/// `IterationBoundaryFacts` bundle needs. Repetition produces these
/// from `attempt_parse_no_edits` (`firstStrictAccepts` is
/// `noInsertAttempt.candidate.matched && !candidate.hadEdits`) and a
/// recoverable probe (`firstRecoverableAccepts` is
/// `probe_recoverable_at_entry(element, ctx)`).
struct IterationElementProbeResult {
  /// True iff the element accepts strictly at the current cursor.
  bool firstStrictAccepts = false;
  /// True iff the element accepts in recoverable mode at the current
  /// cursor.
  bool firstRecoverableAccepts = false;
};

/// Returns true iff a committed recovery prefix imposes that the
/// iteration keep replaying — i.e. there are more committed edits to
/// replay before the iteration can decide to stop. This is the
/// `committedPrefixImposes` fact of `IterationBoundaryFacts`.
[[nodiscard]] inline bool
committed_prefix_imposes_continuation(const RecoveryContext &ctx) noexcept {
  return ctx.committedRecoveryEditIndex < ctx.committedRecoveryEdits.size();
}

/// Builds the closed `IterationBoundaryFacts` bundle from the
/// recovery context and the per-element probes.
///
/// Inputs:
///   - `ctx`            — non-const because the follow probes call
///                        through function pointers stored on the
///                        context. The probes do not mutate the
///                        cursor; they read the recovery state to
///                        report whether the parent's follow accepts
///                        here.
///   - `iterationStarted` — typically
///                        `IterationObservation::iterationStarted` or
///                        equivalent; true iff the iteration has
///                        begun strictly.
///   - `probes`         — the element-level first-set probes the
///                        caller has already computed.
[[nodiscard]] inline IterationBoundaryFacts
make_iteration_boundary_facts(
    RecoveryContext &ctx, bool iterationStarted,
    const IterationElementProbeResult &probes) noexcept {
  return IterationBoundaryFacts{
      .startedStrictly = iterationStarted,
      .firstStrict = probes.firstStrictAccepts,
      .firstRecoverable = probes.firstRecoverableAccepts,
      .followStrict = ctx.probeFollowAcceptsHere(),
      .followRecoverable = ctx.probeRecoverableFollowHere(),
      .committedPrefixImposes = committed_prefix_imposes_continuation(ctx),
  };
}

} // namespace pegium::parser::detail
