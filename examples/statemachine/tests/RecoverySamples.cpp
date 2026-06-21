#include <gtest/gtest.h>

#include <statemachine/core/Parser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace statemachine::tests {
namespace {

std::string summarize_states(const ast::Statemachine &model) {
  std::string summary;
  for (const auto &state : model.states) {
    if (!summary.empty()) {
      summary += " | ";
    }
    summary += state ? state->name : "<null>";
  }
  return summary;
}

void validate_sample(const pegium::test::NamedSampleFile &sample,
                     const pegium::workspace::Document &document) {
  const auto &parsed = document.parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  auto *model = dynamic_cast<const ast::Statemachine *>(parsed.value);
  ASSERT_NE(model, nullptr) << sample.label << " :: " << parseDump;

  if (sample.label == "missing_arrow_character.statemachine") {
    EXPECT_EQ(model->states.size(), 4u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    return;
  }

  if (sample.label == "missing_state_end_eof.statemachine") {
    ASSERT_EQ(model->states.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    auto *idle = model->states.front();
    ASSERT_NE(idle, nullptr);
    ASSERT_EQ(idle->transitions.size(), 1u);
    auto *transition = idle->transitions.front();
    ASSERT_NE(transition, nullptr);
    EXPECT_EQ(transition->event.getRefText(), "Start");
    EXPECT_EQ(transition->state.getRefText(), "Idle");
    return;
  }

  if (sample.label == "close_missing_commands_keyword_char.statemachine") {
    ASSERT_EQ(model->commands.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    ASSERT_EQ(model->states.size(), 1u);
    auto *idle = model->states.front();
    ASSERT_NE(idle, nullptr);
    EXPECT_EQ(idle->name, "Idle");
    return;
  }

  if (sample.label == "close_missing_state_keyword_char.statemachine") {
    ASSERT_EQ(model->states.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    auto *idle = model->states.front();
    ASSERT_NE(idle, nullptr);
    EXPECT_EQ(idle->name, "Idle");
    return;
  }

  if (sample.label == "long_garbage_inside_state.statemachine") {
    ASSERT_EQ(model->states.size(), 1u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    auto *idle = model->states.front();
    ASSERT_NE(idle, nullptr);
    EXPECT_EQ(idle->name, "Idle");
    ASSERT_EQ(idle->transitions.size(), 1u);
    ASSERT_NE(idle->transitions.front(), nullptr);
    EXPECT_EQ(idle->transitions.front()->event.getRefText(), "Start");
    EXPECT_EQ(idle->transitions.front()->state.getRefText(), "Idle");
    return;
  }

  if (sample.label == "multiple_malformed_transitions.statemachine") {
    // The input contains a malformed transition (`Start ==> Running`), an
    // unrecovered transition (`Stop > Idle`) and a clean trailing state
    // declaration (`state Running end`). The recovery currently fixes the
    // first transition, closes Idle prematurely, drops the second
    // transition, and recovers the trailing state — yielding two states.
    // Re-recovering the second `>` as `=>` would let Idle keep both
    // transitions; tracked as a known limitation of the per-iteration
    // Repetition recovery (it does not currently re-spend an Insert edit
    // after the previous iteration already consumed one).
    ASSERT_EQ(model->states.size(), 2u)
        << sample.label << " :: " << parseDump << " :: "
        << summarize_states(*model);
    auto *idle = model->states.front();
    ASSERT_NE(idle, nullptr);
    EXPECT_EQ(idle->name, "Idle");
    EXPECT_GE(idle->transitions.size(), 1u);
    EXPECT_NE(idle->transitions.front(), nullptr);
    auto *running = model->states.back();
    ASSERT_NE(running, nullptr);
    EXPECT_EQ(running->name, "Running");
  }
}

std::vector<pegium::test::NamedSampleFile> recovery_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "recovery-samples",
      {".statemachine"});
}

class StatemachineRecoverySampleTest
    : public ::testing::TestWithParam<pegium::test::NamedSampleFile> {};

TEST(StatemachineRecoverySampleCorpusTest, IsNotEmpty) {
  EXPECT_FALSE(recovery_samples().empty());
}

TEST_P(StatemachineRecoverySampleTest, RecoversCompletely) {
  const auto &sample = GetParam();

  parser::StateMachineParser parser;
  auto document = pegium::test::parse_document(
      parser, pegium::test::read_text_file(sample.path),
      pegium::test::make_file_uri(sample.label), "statemachine");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered)
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(parsed.parseDiagnostics.empty())
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << sample.label << " :: " << parseDump;
  validate_sample(sample, *document);
}

INSTANTIATE_TEST_SUITE_P(
    RecoverySamples, StatemachineRecoverySampleTest,
    ::testing::ValuesIn(recovery_samples()),
    [](const ::testing::TestParamInfo<pegium::test::NamedSampleFile> &info) {
      return info.param.testName;
    });

} // namespace
} // namespace statemachine::tests
