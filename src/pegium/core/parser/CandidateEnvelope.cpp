#include <pegium/core/parser/CandidateEnvelope.hpp>

namespace pegium::parser::detail {

CandidateEnvelope to_candidate_envelope(const EditableRecoveryCandidate &candidate,
                                        CandidateOrigin origin) noexcept {
  return {
      .key = editable_recovery_key(candidate),
      .contract = {.replayPrefix = candidate.replayPrefix},
      .origin = origin,
  };
}

CandidateEnvelope
to_candidate_envelope(const StructuralProgressRecoveryCandidate &candidate,
                      CandidateOrigin origin) noexcept {
  return {
      .key = structural_progress_recovery_key(candidate),
      .contract = {.replayPrefix = candidate.replayPrefix},
      .origin = origin,
  };
}

bool is_extending_anchor_pair(const CandidateEnvelope &next,
                              const CandidateEnvelope &current) noexcept {
  return next.origin == current.origin &&
         next.contract.replayPrefix ==
             ReplayPrefixClass::ExtendedCommittedPrefix &&
         current.contract.replayPrefix != ReplayPrefixClass::Empty &&
         next.key.firstEditOffset == current.key.firstEditOffset &&
         next.key.editCost >= current.key.editCost &&
         next.key.progressAfterEdits > current.key.progressAfterEdits;
}

bool extension_outranks_anchor_base(const CandidateEnvelope &next,
                                    const CandidateEnvelope &current) noexcept {
  if (!is_extending_anchor_pair(next, current)) {
    return false;
  }
  if (current.key.progressAfterEdits > current.key.firstEditOffset) {
    const auto costGap = next.key.editCost - current.key.editCost;
    const auto progressGap =
        next.key.progressAfterEdits - current.key.progressAfterEdits;
    return progressGap >= costGap;
  }
  return true;
}

bool boundary_repair_outranks_no_edit_iteration(
    const CandidateEnvelope &candidate,
    const CandidateEnvelope &committed) noexcept {
  if (candidate.origin != committed.origin) {
    return false;
  }
  return candidate.contract.replayPrefix != ReplayPrefixClass::Empty &&
         committed.contract.replayPrefix == ReplayPrefixClass::Empty &&
         candidate.key.progressAfterEdits > committed.key.progressAfterEdits;
}

bool destructive_extension_outranks_anchor_base(
    const CandidateEnvelope &next,
    const CandidateEnvelope &current) noexcept {
  if (!is_extending_anchor_pair(next, current)) {
    return false;
  }
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
