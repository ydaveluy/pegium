#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::fuzz {

struct ScenarioDocumentSpec {
  std::string uri;
  std::string languageId;
  std::string text;
};

struct WorkspaceScenarioSpec {
  std::string name;
  std::vector<ScenarioDocumentSpec> documents;
};

[[nodiscard]] const std::vector<WorkspaceScenarioSpec> &
stress_single_document_scenarios();

[[nodiscard]] const std::vector<WorkspaceScenarioSpec> &
adversarial_single_document_scenarios();

[[nodiscard]] const std::vector<WorkspaceScenarioSpec> &
adversarial_workspace_scenarios();

void expect_workspace_round_trip(const WorkspaceScenarioSpec &scenario,
                                 std::size_t targetIndex,
                                 std::string_view mutationProgram);

void expect_cached_workspace_round_trip(const WorkspaceScenarioSpec &scenario,
                                        std::size_t targetIndex,
                                        std::string_view mutationProgram);

/// Apply a sequence of mutation programs to one document of the scenario,
/// driving successive `didChange`-style updates without restoring between
/// steps. Each mutation operates on the current (possibly already-mutated)
/// document text, mirroring an LSP user's editing burst. After the final
/// step the runner restores the baseline text and asserts that the resulting
/// snapshot matches the scenario's baseline (round-trip stability).
///
/// Each program in `mutationPrograms` is interpreted by `mutate_text` (up to
/// 12 byte-coded operations per program). Empty programs are skipped.
void expect_workspace_incremental_chain(
    const WorkspaceScenarioSpec &scenario, std::size_t targetIndex,
    const std::vector<std::string> &mutationPrograms);

void expect_stress_document_build(std::string_view text);

void expect_adversarial_document_build(std::string_view text);

} // namespace pegium::fuzz
