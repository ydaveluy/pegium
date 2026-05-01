#pragma once

/// Centralised default values for recovery policy.
///
/// Every numeric default that controls recovery behaviour is declared here
/// so that it can be referenced by all contexts (ParseContext, ExpectContext,
/// ContextShared::EditCheckpointState, Parser::ParseOptions) without drift.

#include <cstdint>

namespace pegium::parser {

/// Cap on a single contiguous delete run during recovery. Replicated as the
/// initial value of `maxConsecutiveCodepointDeletes` on every context that
/// participates in recovery. Increase only with care: larger values let
/// delete-scan recovery span more visible noise but also raise the risk of
/// crossing a structural boundary.
inline constexpr std::uint32_t kDefaultMaxConsecutiveCodepointDeletes = 8;

/// Observation budget for InfixRule's RHS noise probe. Capped strictly below
/// `maxConsecutiveCodepointDeletes` so the speculative scan cannot grow with
/// the global delete budget and reach across statement boundaries (e.g. it
/// must not gobble the next statement's leading identifier as the rhs
/// primary). The replay still respects `maxConsecutiveCodepointDeletes`;
/// this constant controls only the upfront observation.
inline constexpr std::uint32_t kInfixOperatorNoiseObservationDeleteCap = 4;

} // namespace pegium::parser
