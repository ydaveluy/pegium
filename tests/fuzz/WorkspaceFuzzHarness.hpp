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

void expect_stress_document_build(std::string_view text);

void expect_adversarial_document_build(std::string_view text);

} // namespace pegium::fuzz
