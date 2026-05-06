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

/// The contract bundle. Reduced to the single axis that admission and
/// dominance predicates actually consume.
struct RecoveryContract {
  ReplayPrefixClass replayPrefix = ReplayPrefixClass::Empty;

  [[nodiscard]] friend bool operator==(const RecoveryContract &a,
                                        const RecoveryContract &b) noexcept {
    return a.replayPrefix == b.replayPrefix;
  }
};

static_assert(std::is_trivially_copyable_v<RecoveryContract>,
              "RecoveryContract must remain a POD: it is captured in cache "
              "keys, dominance comparisons, and test snapshots, all of which "
              "depend on cheap value semantics.");

} // namespace pegium::parser::detail
