#pragma once

/// Centralised default values for recovery policy.
///
/// Every numeric default that controls recovery behaviour is declared here
/// so that it can be referenced by all contexts (ParseContext, ExpectContext,
/// detail::EditCheckpointState, Parser::ParseOptions) without drift.

#include <cstdint>

namespace pegium::parser {

/// Cap on a single contiguous delete run during recovery. Replicated as the
/// initial value of `maxConsecutiveCodepointDeletes` on every context that
/// participates in recovery. Increase only with care: larger values let
/// delete-scan recovery span more visible noise but also raise the risk of
/// crossing a structural boundary.
inline constexpr std::uint32_t kDefaultMaxConsecutiveCodepointDeletes = 8;

/// Maximum bytes a `Repetition` may skip when no normal recovery plan can
/// bridge the current iteration (last-resort panic-mode budget). Shared by the
/// `ParseOptions` default and the `RecoveryContext` per-window limit so they
/// cannot drift.
inline constexpr std::uint32_t kDefaultMaxResyncSkipCodepoints = 4096;

/// Number of strict visible leaves that must parse after a recovery edit before
/// the edit is considered stable. Shared by the `ParseOptions` default and the
/// `RecoveryContext` per-context value so they cannot drift.
inline constexpr std::uint32_t kDefaultRecoveryStabilityTokenCount = 2;

} // namespace pegium::parser
