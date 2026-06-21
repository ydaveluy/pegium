/// `CandidateEnvelope` tests.
///
/// The envelope is the small common type that admission, dominance,
/// and ranking all read. This suite covers:
///
///   1. The envelope is a POD with deterministic defaults.
///   2. `to_candidate_envelope` projects an `EditableRecoveryCandidate` onto
///      the envelope, reproducing `editable_recovery_key` exactly.
///   3. The ranking key reads the producer-chosen `keyProgressOffset`
///      (OrderedChoice/Group use the post-skip cursor; Repetition uses the raw
///      iteration cursor — Invariant 2), both for `progressAfterEdits` and the
///      no-edit first-edit fallback.

#include <pegium/core/parser/CandidateEnvelope.hpp>
#include <pegium/core/parser/RecoveryCandidate.hpp>

#include <gtest/gtest.h>

#include <type_traits>

using pegium::parser::detail::CandidateEnvelope;
using pegium::parser::detail::editable_recovery_key;
using pegium::parser::detail::EditableRecoveryCandidate;
using pegium::parser::detail::is_better_recovery_key;
using pegium::parser::detail::to_candidate_envelope;

// -----------------------------------------------------------------------------
// 1. Envelope struct shape
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeStruct, default_envelope_is_keyless) {
  const CandidateEnvelope envelope;
  EXPECT_FALSE(envelope.key.matched);
  EXPECT_EQ(envelope.key.editCost, 0U);
}

TEST(CandidateEnvelopeStruct, envelope_is_trivially_copyable) {
  static_assert(std::is_trivially_copyable_v<CandidateEnvelope>);
}

// -----------------------------------------------------------------------------
// 2. Mapping reproduces editable_recovery_key
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping, mapping_reproduces_editable_recovery_key) {
  EditableRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.postSkipCursorOffset = 35;
  candidate.keyProgressOffset = 35;
  candidate.firstEditOffset = 10;
  candidate.lastEditEndOffset = 15;
  candidate.editCost = 8;
  candidate.editCount = 2;

  const auto envelope = to_candidate_envelope(candidate);
  const auto key = editable_recovery_key(candidate);
  EXPECT_EQ(envelope.key.matched, key.matched);
  EXPECT_EQ(envelope.key.firstEditOffset, key.firstEditOffset);
  EXPECT_EQ(envelope.key.editCost, key.editCost);
  EXPECT_EQ(envelope.key.editCount, key.editCount);
  EXPECT_EQ(envelope.key.progressAfterEdits, key.progressAfterEdits);
}

// -----------------------------------------------------------------------------
// 3. The ranking key reads keyProgressOffset (the producer-chosen progress axis)
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping, key_progress_offset_drives_progress_after_edits) {
  // Repetition sets keyProgressOffset to the RAW iteration cursor, distinct
  // from postSkipCursorOffset. The ranking key must read keyProgressOffset.
  EditableRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.editCost = 4;
  candidate.editCount = 1;
  candidate.firstEditOffset = 10;
  candidate.postSkipCursorOffset = 28;
  candidate.keyProgressOffset = 30;

  EXPECT_EQ(editable_recovery_key(candidate).progressAfterEdits, 30u);
}

TEST(CandidateEnvelopeMapping,
     no_edit_candidate_folds_progress_into_first_edit) {
  // With no edits, firstEditOffset is the sentinel; the key must fold the
  // first-edit axis onto keyProgressOffset so a no-edit candidate does not
  // unfairly win axis 3 over an insert-and-continue candidate.
  EditableRecoveryCandidate candidate;
  candidate.matched = true;
  candidate.postSkipCursorOffset = 30;
  candidate.keyProgressOffset = 30;
  // no edits: firstEditOffset stays at sentinel, editCount 0.

  const auto key = editable_recovery_key(candidate);
  EXPECT_EQ(key.firstEditOffset, 30u);
  EXPECT_EQ(key.progressAfterEdits, 30u);
}

// -----------------------------------------------------------------------------
// 4. Mapped envelope ranks identically to the candidate key under
//    is_better_recovery_key.
// -----------------------------------------------------------------------------

TEST(CandidateEnvelopeMapping,
     ranking_via_envelope_key_matches_pairwise_decision) {
  EditableRecoveryCandidate alpha;
  alpha.matched = true;
  alpha.firstEditOffset = 10;
  alpha.editCost = 4;
  alpha.editCount = 1;
  alpha.postSkipCursorOffset = 30;
  alpha.keyProgressOffset = 30;

  EditableRecoveryCandidate beta;
  beta.matched = true;
  beta.firstEditOffset = 5;
  beta.editCost = 4;
  beta.editCount = 1;
  beta.postSkipCursorOffset = 25;
  beta.keyProgressOffset = 25;

  const auto alphaEnvelope = to_candidate_envelope(alpha);
  const auto betaEnvelope = to_candidate_envelope(beta);

  const bool keyDecision = is_better_recovery_key(
      editable_recovery_key(alpha), editable_recovery_key(beta));
  const bool envelopeDecision =
      is_better_recovery_key(alphaEnvelope.key, betaEnvelope.key);

  EXPECT_EQ(envelopeDecision, keyDecision);
}
