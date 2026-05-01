/// Sanity test for the observe/rewind harness.
///
/// The harness lives in `RecoveryHarnessTestSupport.hpp`. This file
/// verifies the snapshot mechanism end to end: two snapshots taken
/// on identical state must compare equal, a changed cursor must
/// produce a different snapshot, and a checkpoint/mutation/rewind
/// cycle must restore a matching snapshot.

#include <pegium/core/RecoveryHarnessTestSupport.hpp>
#include <pegium/core/parser/ContextShared.hpp>
#include <pegium/core/parser/ParseContext.hpp>
#include <pegium/core/parser/SkipperBuilder.hpp>
#include <pegium/core/syntax-tree/CstBuilder.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/text/TextSnapshot.hpp>

#include <gtest/gtest.h>

using namespace pegium::parser;
using namespace pegium::test;

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

} // namespace

TEST(RecoveryObserveRewindHarness, snapshot_of_unchanged_context_is_stable) {
  ContextFixture fixture;
  const auto a = capture_recovery_context_snapshot(fixture.ctx);
  const auto b = capture_recovery_context_snapshot(fixture.ctx);
  EXPECT_EQ(a, b);
  EXPECT_EQ(a.hash, b.hash);
}

TEST(RecoveryObserveRewindHarness, snapshot_diverges_when_recovery_edit_appended) {
  ContextFixture fixture;
  const auto before = capture_recovery_context_snapshot(fixture.ctx);
  fixture.ctx.recoveryEdits.push_back(
      {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
       .offset = 0,
       .beginOffset = 0,
       .endOffset = 0});
  const auto after = capture_recovery_context_snapshot(fixture.ctx);
  EXPECT_NE(before, after);
}

TEST(RecoveryObserveRewindHarness, observe_rewind_neutral_on_noop_observation) {
  ContextFixture fixture;
  expect_observe_rewind_neutral(
      fixture.ctx,
      [](RecoveryContext & /*ctx*/) {
        // Pure no-op observation.
      },
      "noop_observation");
}

TEST(RecoveryObserveRewindHarness,
     observe_rewind_neutral_when_recovery_edit_appended_then_rewound) {
  ContextFixture fixture;
  expect_observe_rewind_neutral(
      fixture.ctx,
      [](RecoveryContext &ctx) {
        ctx.recoveryEdits.push_back(
            {.kind = pegium::parser::ParseDiagnosticKind::Inserted,
             .offset = 0,
             .beginOffset = 0,
             .endOffset = 0});
      },
      "append_recovery_edit_then_rewind");
}
