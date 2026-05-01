/// `CandidateEnvelope` tests.
///
/// The envelope is the small common type that admission, dominance,
/// and ranking all read. This suite covers:
///
///   1. The envelope and its evidence are PODs with deterministic
///      defaults.
///   2. The legacy `EditableRecoveryCandidate` maps cleanly to an
///      envelope, preserving the existing ranking key semantics.
///   3. Evidence is observation-only: the envelope does not consult
///      its evidence to compute the ranking key (the key matches the
///      legacy `editable_recovery_key` output exactly).
///
/// The closedness of `CandidateOrigin` is enforced by the compiler at
/// every dispatch site (`-Wswitch` warnings promoted to errors).

#include <pegium/core/parser/CandidateEnvelope.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <type_traits>

using pegium::TextOffset;
using pegium::parser::detail::CandidateEnvelope;
using pegium::parser::detail::CandidateOrigin;
using pegium::parser::detail::editable_recovery_key;
using pegium::parser::detail::EditableRecoveryCandidate;
using pegium::parser::detail::is_better_recovery_key;
using pegium::parser::detail::to_candidate_envelope;

// -----------------------------------------------------------------------------
// 1. Envelope struct shape
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeStruct, default_envelope_is_unspecified_and_keyless) {
  const CandidateEnvelope envelope;
  EXPECT_EQ(envelope.origin, CandidateOrigin::Unspecified);
  EXPECT_FALSE(envelope.key.matched);
  EXPECT_EQ(envelope.key.editCost, 0U);
}

TEST(CandidateEnvelopeStruct, envelope_is_trivially_copyable) {
  static_assert(std::is_trivially_copyable_v<CandidateEnvelope>);
}

// -----------------------------------------------------------------------------
// 3. Mapping from EditableRecoveryCandidate
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping,
     mapping_preserves_legacy_recovery_key_for_no_edit_candidate) {
  EditableRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.cursorOffset = 42;
  candidate.postSkipCursorOffset = 50;

  const auto envelope =
      to_candidate_envelope(candidate, CandidateOrigin::OrderedChoiceBranch);
  const auto legacy = editable_recovery_key(candidate);

  EXPECT_EQ(envelope.key.matched, legacy.matched);
  EXPECT_EQ(envelope.key.firstEditOffset, legacy.firstEditOffset);
  EXPECT_EQ(envelope.key.editCost, legacy.editCost);
  EXPECT_EQ(envelope.key.progressAfterEdits, legacy.progressAfterEdits);
  EXPECT_EQ(envelope.origin, CandidateOrigin::OrderedChoiceBranch);
}

TEST(CandidateEnvelopeMapping,
     mapping_preserves_legacy_recovery_key_for_edit_candidate) {
  EditableRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.cursorOffset = 30;
  candidate.postSkipCursorOffset = 35;
  candidate.firstEditOffset = 10;
  candidate.editSpan = 5;
  candidate.editCost = 8;
  candidate.editCount = 2;

  const auto envelope = to_candidate_envelope(candidate);
  const auto legacy = editable_recovery_key(candidate);

  EXPECT_EQ(envelope.key.matched, legacy.matched);
  EXPECT_EQ(envelope.key.firstEditOffset, legacy.firstEditOffset);
  EXPECT_EQ(envelope.key.editCost, legacy.editCost);
  EXPECT_EQ(envelope.key.progressAfterEdits, legacy.progressAfterEdits);
}

TEST(CandidateEnvelopeMapping,
     mapping_default_origin_is_unspecified) {
  EditableRecoveryCandidate candidate;
  const auto envelope = to_candidate_envelope(candidate);
  EXPECT_EQ(envelope.origin, CandidateOrigin::Unspecified);
}

// -----------------------------------------------------------------------------
// 4. Evidence is observation-only — mapped envelope ranks identically to
//    the legacy candidate when both are compared via is_better_recovery_key.
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping,
     ranking_via_envelope_key_matches_legacy_pairwise_decision) {
  EditableRecoveryCandidate alpha;
  alpha.matched = true;
  alpha.firstEditOffset = 10;
  alpha.editCost = 4;
  alpha.editCount = 1;
  alpha.postSkipCursorOffset = 30;

  EditableRecoveryCandidate beta;
  beta.matched = true;
  beta.firstEditOffset = 5;
  beta.editCost = 4;
  beta.editCount = 1;
  beta.postSkipCursorOffset = 25;

  const auto alphaEnvelope = to_candidate_envelope(alpha);
  const auto betaEnvelope = to_candidate_envelope(beta);

  const bool legacyDecision =
      is_better_recovery_key(editable_recovery_key(alpha),
                             editable_recovery_key(beta));
  const bool envelopeDecision =
      is_better_recovery_key(alphaEnvelope.key, betaEnvelope.key);

  EXPECT_EQ(envelopeDecision, legacyDecision);
}

// -----------------------------------------------------------------------------
// 5. StructuralProgressRecoveryCandidate adapter
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping,
     structural_progress_mapping_default_origin_is_repetition_iteration) {
  pegium::parser::detail::StructuralProgressRecoveryCandidate candidate;
  const auto envelope = to_candidate_envelope(candidate);
  EXPECT_EQ(envelope.origin, CandidateOrigin::RepetitionIteration);
}

TEST(CandidateEnvelopeMapping,
     structural_progress_mapping_preserves_legacy_recovery_key_no_edits) {
  pegium::parser::detail::StructuralProgressRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.cursorOffset = 30;
  candidate.editCost = 0;
  candidate.hadEdits = false;
  // firstEditOffset stays at sentinel.
  const auto envelope = to_candidate_envelope(candidate);
  const auto legacy =
      pegium::parser::detail::structural_progress_recovery_key(candidate);
  EXPECT_EQ(envelope.key.matched, legacy.matched);
  EXPECT_EQ(envelope.key.firstEditOffset, legacy.firstEditOffset);
  EXPECT_EQ(envelope.key.editCost, legacy.editCost);
  EXPECT_EQ(envelope.key.progressAfterEdits, legacy.progressAfterEdits);
}

TEST(CandidateEnvelopeMapping,
     structural_progress_ranking_via_envelope_matches_legacy) {
  pegium::parser::detail::StructuralProgressRecoveryCandidate alpha;
  alpha.matched = true;
  alpha.cursorOffset = 30;
  alpha.firstEditOffset = 10;
  alpha.editCost = 4;
  alpha.hadEdits = true;

  pegium::parser::detail::StructuralProgressRecoveryCandidate beta;
  beta.matched = true;
  beta.cursorOffset = 25;
  beta.firstEditOffset = 5;
  beta.editCost = 4;
  beta.hadEdits = true;

  const auto alphaEnvelope = to_candidate_envelope(alpha);
  const auto betaEnvelope = to_candidate_envelope(beta);

  const bool legacyDecision = is_better_recovery_key(
      pegium::parser::detail::structural_progress_recovery_key(alpha),
      pegium::parser::detail::structural_progress_recovery_key(beta));
  const bool envelopeDecision =
      is_better_recovery_key(alphaEnvelope.key, betaEnvelope.key);

  EXPECT_EQ(envelopeDecision, legacyDecision);
}
