#pragma once

/// `IterationBoundaryDecision`: the closed boundary decision a
/// `Repetition` makes once, before it enumerates any local recovery
/// candidate.
///
/// Every `Repetition` recovery attempt selects exactly one
/// `IterationBoundaryDecision` from the closed precedence table below
/// before producing any candidate. The decision determines which
/// candidate families the iteration is even allowed to enumerate:
///
///   - `StopCleanly`              no local edit candidate
///   - `ContinueStrict`           no-edit strict continuation only
///   - `RepairStartedIteration`   delete/insert retry on an iteration
///                                that already started
///   - `Resync`                   last-resort recovery
///   - `RejectToParentFollow`     no local candidate; defer to parent
///
/// The decision is computed from 4 boolean structural facts only —
/// budget concerns, candidate enumeration, and admission rules live
/// outside the precedence table. This isolation is what makes the
/// table closed and exhaustively testable.
///
/// Strict-path discipline: this header is recovery-side. It must
/// never be constructed on the strict-only nominal path.

#include <cstdint>
#include <type_traits>

namespace pegium::parser::detail {

/// Closed list of decisions for the `Repetition` boundary precedence
/// table.
enum class IterationBoundaryDecision : std::uint8_t {
  /// No local edit candidate is allowed. The iteration stops without
  /// recovery; control returns to the parent.
  StopCleanly,

  /// The iteration continues strictly: the element accepts at the
  /// current cursor without any edit. No edit candidate is produced.
  ContinueStrict,

  /// The iteration was already started strictly and a local repair
  /// can be observed without crossing the strict parent follow.
  /// Delete/insert retry candidates on the started iteration are
  /// admissible.
  RepairStartedIteration,

  /// Last-resort recovery: no other rule applies, no strict boundary
  /// is in scope, the iteration falls back to a budgeted resync scan.
  Resync,

  /// The only producible local candidate would cross the strict
  /// parent follow. The iteration produces nothing locally and
  /// defers to the parent.
  RejectToParentFollow,
};

/// The 4 closed structural facts the precedence table reads. These
/// are the only inputs to `decide_iteration_boundary`.
struct IterationBoundaryFacts {
  /// True iff the iteration has consumed at least one strict leaf
  /// without recovery edit (the iteration is mid-flight).
  bool startedStrictly = false;

  /// True iff the element accepts strictly at the current cursor.
  bool firstStrict = false;

  /// True iff the parent follow accepts strictly at the current
  /// cursor.
  bool followStrict = false;

  /// True iff the committed prefix forces the iteration to keep
  /// going (i.e. `StopCleanly` is inadmissible until the committed
  /// replay closes).
  bool committedPrefixImposes = false;

  [[nodiscard]] friend bool
  operator==(const IterationBoundaryFacts &a,
             const IterationBoundaryFacts &b) noexcept = default;
};

static_assert(std::is_trivially_copyable_v<IterationBoundaryFacts>);
static_assert(sizeof(IterationBoundaryFacts) <= 4);

/// Computes the iteration boundary decision from the 4 facts.
///
/// The implementation is the closed precedence table:
///
///   1. `firstStrict`                        -> ContinueStrict
///   2. `followStrict && !committedImposes`  -> StopCleanly
///   3. `startedStrictly && !followStrict`   -> RepairStartedIteration
///   4. `!followStrict`                      -> Resync
///   5. otherwise                            -> RejectToParentFollow
///
/// The function is total and exclusive on the 16 combinations of the
/// 4 facts: every combination receives exactly one decision. This is
/// the property the exhaustive test pins.
///
/// Inputs the table does NOT read on purpose:
///   - resync budget: caller's responsibility; if `Resync` is
///     returned but the budget refuses to enumerate one, the caller
///     falls back to `StopCleanly` (or `RejectToParentFollow` when
///     the committed prefix imposes).
///   - candidate enumeration cost: the table decides which families
///     are allowed; the caller produces the actual candidates.
[[nodiscard]] constexpr IterationBoundaryDecision
decide_iteration_boundary(IterationBoundaryFacts facts) noexcept {
  if (facts.firstStrict) {
    return IterationBoundaryDecision::ContinueStrict;
  }
  if (facts.followStrict && !facts.committedPrefixImposes) {
    return IterationBoundaryDecision::StopCleanly;
  }
  if (!facts.followStrict) {
    if (facts.startedStrictly) {
      return IterationBoundaryDecision::RepairStartedIteration;
    }
    return IterationBoundaryDecision::Resync;
  }
  // followStrict && committedPrefixImposes: the strict parent follow
  // would block any local repair, but the committed prefix forbids
  // a clean stop. Defer to the parent.
  return IterationBoundaryDecision::RejectToParentFollow;
}

} // namespace pegium::parser::detail
