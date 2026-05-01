#pragma once

/// `RecoveryContract`: closed-enum vocabulary describing what a
/// recovery candidate observes about its position in the parse, free
/// of any preference information.
///
/// The contract is a vocabulary, not a ranking. Preferences live in
/// `RecoveryKey`, not here.

#include <cstdint>
#include <type_traits>

namespace pegium::parser::detail {

/// Continuation the candidate must demonstrate after its last edit
/// before being admissible.
enum class ContinuationRequirement : std::uint8_t {
  /// No continuation needed (e.g. candidate ends at EOF or accepts
  /// strictly without edits).
  None,
  /// The candidate must lead into a strictly-accepting continuation.
  StrictContinuation,
  /// The candidate may lead into a recoverable continuation.
  RecoverableContinuation,
  /// The candidate must show at least one visible non-synthetic leaf
  /// consumed strictly after `lastEditOffset` within the current site.
  /// This is the lazy `visibleContinuationAfterEdit` query.
  VisibleContinuationAfterEdit,
  /// The candidate ends at EOF.
  EofContinuation,
};

/// Class of the candidate's replay prefix relative to the committed
/// prefix already applied at the site. Necessary but never sufficient
/// for replay-equivalence.
enum class ReplayPrefixClass : std::uint8_t {
  /// The candidate has no edits to replay.
  Empty,
  /// The candidate's replay prefix matches the committed prefix
  /// exactly.
  SameCommittedPrefix,
  /// The candidate's replay prefix is the committed prefix plus
  /// strictly later additional edits.
  ExtendedCommittedPrefix,
  /// The candidate's replay prefix is a new local prefix, not derived
  /// from the committed one.
  NewLocalPrefix,
};

/// The contract bundle. Reduced to the two axes that admission and
/// dominance predicates actually consume.
struct RecoveryContract {
  ContinuationRequirement continuation = ContinuationRequirement::None;
  ReplayPrefixClass replayPrefix = ReplayPrefixClass::Empty;

  [[nodiscard]] friend bool operator==(const RecoveryContract &a,
                                        const RecoveryContract &b) noexcept {
    return a.continuation == b.continuation &&
           a.replayPrefix == b.replayPrefix;
  }
};

static_assert(std::is_trivially_copyable_v<RecoveryContract>,
              "RecoveryContract must remain a POD: it is captured in cache "
              "keys, dominance comparisons, and test snapshots, all of which "
              "depend on cheap value semantics.");

} // namespace pegium::parser::detail
