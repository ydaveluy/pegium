/// `IterationBoundaryFactsBuilder` tests.
///
/// The projection helper maps the recovery context state to the
/// closed `IterationBoundaryFacts` bundle the precedence table reads.
///
/// This suite covers the helper itself:
///
///   1. `committed_prefix_imposes_continuation` projects the context
///      committed-edit cursor correctly.
///   2. `make_iteration_boundary_facts` faithfully transcribes the
///      6 facts: `startedStrictly` from the iteration observation,
///      `firstStrict` / `firstRecoverable` from the element probes,
///      `followStrict` / `followRecoverable` from the context follow
///      probes, `committedPrefixImposes` from the committed-edit
///      cursor.
///   3. Calling the helper does not mutate the context (verified via
///      the observe/rewind harness).

#include <pegium/core/RecoveryHarnessTestSupport.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/parser/IterationBoundaryFactsBuilder.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

#include <gtest/gtest.h>

using namespace pegium::parser;
using pegium::parser::detail::committed_prefix_imposes_continuation;
using pegium::parser::detail::IterationElementProbeResult;
using pegium::parser::detail::make_iteration_boundary_facts;

namespace {

struct ContextFixture {
  pegium::text::TextSnapshot snapshot =
      pegium::text::TextSnapshot::copy("hello world");
  std::unique_ptr<pegium::RootCstNode> cst =
      std::make_unique<pegium::RootCstNode>(snapshot);
  pegium::parser::detail::FailureHistoryRecorder failureRecorder{
      snapshot.view().data()};
  pegium::CstBuilder builder{*cst};
  Skipper skipper = SkipperBuilder().build();
  RecoveryContext ctx{builder, skipper, failureRecorder};
};

RecoveryContext::FollowProbeFn always_accept_probe = +[](RecoveryContext &,
                                                         const void *) -> bool {
  return true;
};

RecoveryContext::FollowProbeFn always_reject_probe = +[](RecoveryContext &,
                                                         const void *) -> bool {
  return false;
};

} // namespace

// -----------------------------------------------------------------------------
// 1. committed_prefix_imposes_continuation
// -----------------------------------------------------------------------------

TEST(IterationBoundaryFactsBuilder,
     committed_prefix_does_not_impose_on_fresh_context) {
  ContextFixture fixture;
  EXPECT_FALSE(committed_prefix_imposes_continuation(fixture.ctx));
}

TEST(IterationBoundaryFactsBuilder,
     committed_prefix_imposes_when_replay_index_lags) {
  ContextFixture fixture;
  // Inject a synthetic committed edit and reset the replay index to
  // 0: the committed prefix has not been replayed yet.
  fixture.ctx.committedRecoveryEdits.push_back(
      {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
       .offset = 0,
       .beginOffset = 0,
       .endOffset = 0});
  fixture.ctx.committedRecoveryEditIndex = 0;
  EXPECT_TRUE(committed_prefix_imposes_continuation(fixture.ctx));
}

TEST(IterationBoundaryFactsBuilder,
     committed_prefix_does_not_impose_after_full_replay) {
  ContextFixture fixture;
  fixture.ctx.committedRecoveryEdits.push_back(
      {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
       .offset = 0,
       .beginOffset = 0,
       .endOffset = 0});
  // The replay has caught up with all committed edits.
  fixture.ctx.committedRecoveryEditIndex = 1;
  EXPECT_FALSE(committed_prefix_imposes_continuation(fixture.ctx));
}

// -----------------------------------------------------------------------------
// 2. make_iteration_boundary_facts projects all 6 facts
// -----------------------------------------------------------------------------

TEST(IterationBoundaryFactsBuilder,
     started_strictly_field_comes_from_argument) {
  ContextFixture fixture;
  IterationElementProbeResult probes;
  auto factsTrue = make_iteration_boundary_facts(fixture.ctx, /*started=*/true,
                                                  probes);
  auto factsFalse = make_iteration_boundary_facts(
      fixture.ctx, /*started=*/false, probes);
  EXPECT_TRUE(factsTrue.startedStrictly);
  EXPECT_FALSE(factsFalse.startedStrictly);
}

TEST(IterationBoundaryFactsBuilder,
     first_strict_and_recoverable_come_from_probes) {
  ContextFixture fixture;
  IterationElementProbeResult probes;
  probes.firstStrictAccepts = true;
  probes.firstRecoverableAccepts = false;
  const auto facts = make_iteration_boundary_facts(fixture.ctx,
                                                    /*started=*/false, probes);
  EXPECT_TRUE(facts.firstStrict);
  EXPECT_FALSE(facts.firstRecoverable);
}

TEST(IterationBoundaryFactsBuilder,
     follow_facts_come_from_context_follow_probes) {
  ContextFixture fixture;
  IterationElementProbeResult probes;
  // No follow probe installed: both follow facts are false.
  auto factsNoProbe = make_iteration_boundary_facts(fixture.ctx,
                                                     /*started=*/false, probes);
  EXPECT_FALSE(factsNoProbe.followStrict);
  EXPECT_FALSE(factsNoProbe.followRecoverable);
  // Install strict-only follow probe.
  {
    auto guard = fixture.ctx.withFollowProbe(always_accept_probe, nullptr);
    auto facts = make_iteration_boundary_facts(fixture.ctx, /*started=*/false,
                                                probes);
    EXPECT_TRUE(facts.followStrict);
    EXPECT_FALSE(facts.followRecoverable);
  }
  // Install recoverable-only follow probe.
  {
    auto guard = fixture.ctx.withFollowProbe(
        always_reject_probe, nullptr,
        /*recoverableFn=*/always_accept_probe, /*recoverableData=*/nullptr);
    auto facts = make_iteration_boundary_facts(fixture.ctx, /*started=*/false,
                                                probes);
    EXPECT_FALSE(facts.followStrict);
    EXPECT_TRUE(facts.followRecoverable);
  }
}

TEST(IterationBoundaryFactsBuilder,
     committed_prefix_imposes_field_reflects_helper_output) {
  ContextFixture fixture;
  IterationElementProbeResult probes;
  // Fresh context.
  EXPECT_FALSE(make_iteration_boundary_facts(fixture.ctx, false, probes)
                   .committedPrefixImposes);
  // Pending committed replay.
  fixture.ctx.committedRecoveryEdits.push_back(
      {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
       .offset = 0,
       .beginOffset = 0,
       .endOffset = 0});
  fixture.ctx.committedRecoveryEditIndex = 0;
  EXPECT_TRUE(make_iteration_boundary_facts(fixture.ctx, false, probes)
                  .committedPrefixImposes);
}

// -----------------------------------------------------------------------------
// 3. Helper does not mutate context (observation-only)
// -----------------------------------------------------------------------------

TEST(IterationBoundaryFactsBuilder,
     helper_call_is_observe_rewind_neutral) {
  ContextFixture fixture;
  auto guard = fixture.ctx.withFollowProbe(
      always_accept_probe, nullptr,
      /*recoverableFn=*/always_accept_probe, /*recoverableData=*/nullptr);
  pegium::test::expect_observe_rewind_neutral(
      fixture.ctx,
      [](RecoveryContext &ctx) {
        IterationElementProbeResult probes;
        probes.firstStrictAccepts = true;
        probes.firstRecoverableAccepts = true;
        (void)make_iteration_boundary_facts(ctx, /*started=*/true, probes);
      },
      "make_iteration_boundary_facts_no_mutation");
}

// -----------------------------------------------------------------------------
// 4. Combined test: every input fact independently controls its output field
// -----------------------------------------------------------------------------

TEST(IterationBoundaryFactsBuilder,
     all_six_facts_are_independently_controllable) {
  ContextFixture fixture;
  for (int mask = 0; mask < 64; ++mask) {
    fixture.ctx.committedRecoveryEdits.clear();
    fixture.ctx.committedRecoveryEditIndex = 0;
    if ((mask & 0b100000) != 0) {
      fixture.ctx.committedRecoveryEdits.push_back(
          {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
           .offset = 0,
           .beginOffset = 0,
           .endOffset = 0});
    }
    auto guard = fixture.ctx.withFollowProbe(
        ((mask & 0b001000) != 0) ? always_accept_probe : always_reject_probe,
        nullptr,
        ((mask & 0b010000) != 0) ? always_accept_probe : always_reject_probe,
        nullptr);
    IterationElementProbeResult probes;
    probes.firstStrictAccepts = (mask & 0b000010) != 0;
    probes.firstRecoverableAccepts = (mask & 0b000100) != 0;
    const bool startedStrictly = (mask & 0b000001) != 0;
    const auto facts = make_iteration_boundary_facts(fixture.ctx,
                                                      startedStrictly, probes);
    EXPECT_EQ(facts.startedStrictly, (mask & 0b000001) != 0);
    EXPECT_EQ(facts.firstStrict, (mask & 0b000010) != 0);
    EXPECT_EQ(facts.firstRecoverable, (mask & 0b000100) != 0);
    EXPECT_EQ(facts.followStrict, (mask & 0b001000) != 0);
    EXPECT_EQ(facts.followRecoverable, (mask & 0b010000) != 0);
    EXPECT_EQ(facts.committedPrefixImposes, (mask & 0b100000) != 0);
  }
}
