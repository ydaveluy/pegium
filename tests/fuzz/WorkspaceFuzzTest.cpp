#include <fuzz/WorkspaceFuzzHarness.hpp>

#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>

#include <ranges>
#include <string>
#include <tuple>
#include <vector>

namespace pegium::fuzz {
namespace {

[[nodiscard]] std::string byte_string(
    std::initializer_list<unsigned int> bytes) {
  std::string value;
  value.reserve(bytes.size());
  for (const auto byte : bytes) {
    value.push_back(static_cast<char>(byte));
  }
  return value;
}

[[nodiscard]] std::vector<std::string> mutation_programs() {
  return {
      std::string{},
      std::string("\0A;", 3),
      byte_string({0x00, 0x00, 0x00}),
      byte_string({0x00, 0xff, 0x00}),
      byte_string({0x00, 0x00, 0x00, 0x04, 0x00, 0x03}),
      byte_string({0x00, 0x00, 0x00, 0x04, 0x01, 0x00}),
      byte_string({0x00, 0x00, 0x00, 0x00, 0x00, 0x00}),
      byte_string({0x01, 0x00, 0x01, 0x03, 0x00, 0x00}),
      byte_string({0x02, 0x00, 0x00}),
      byte_string({0x02, 0x00, 0x00, 0x04, 0x01, 0x02}),
      byte_string({0x03, 0x00, 0x00}),
      byte_string({0x03, 0x00, 0x00, 0x03, 0x01, 0x00}),
      byte_string({0x04, 0x00, 0x00}),
      byte_string({0x04, 0x00, 0x07}),
      byte_string({0x04, 0x01, 0x00}),
      byte_string({0x04, 0x01, 0x07}),
      byte_string({0x05, 0x01, 0x01, 0x02, 0x00, 0x00}),
      byte_string({0x06, 0x00, 0x00}),
      byte_string({0x06, 0x01, 0x04}),
      byte_string({0x06, 0x00, 0x06}),
      byte_string({0x06, 0x00, 0x0d}),
      byte_string({0x07, 0x00, 0x00}),
      byte_string({0x07, 0x03, 0x04}),
      byte_string({0x07, 0x01, 0x0f}),
      byte_string({0x08, 0x07, 0x00}),
      byte_string({0x08, 0x05, 0x04}),
      byte_string({0x08, 0x0b, 0x0d}),
      byte_string({0x06, 0x00, 0x00, 0x07, 0x01, 0x00}),
      byte_string({0x06, 0x00, 0x04, 0x08, 0x05, 0x04}),
      byte_string({0x07, 0x00, 0x0d, 0x06, 0x01, 0x0e}),
      byte_string({0x06, 0x00, 0x10}),
      byte_string({0x06, 0x01, 0x11}),
      byte_string({0x07, 0x00, 0x12}),
      byte_string({0x07, 0x01, 0x13}),
      byte_string({0x08, 0x10, 0x11}),
      byte_string({0x08, 0x12, 0x13}),
      byte_string({0x07, 0x00, 0x10, 0x07, 0x01, 0x12}),
      byte_string({0x06, 0x00, 0x10, 0x08, 0x11, 0x12}),
      byte_string({0x06, 0x00, 0x0d}),
      byte_string({0x06, 0x01, 0x0e}),
      byte_string({0x07, 0x00, 0x0d}),
      byte_string({0x07, 0x01, 0x0e}),
      byte_string({0x07, 0x01, 0x0f}),
      byte_string({0x06, 0x00, 0x06, 0x06, 0x01, 0x07}),
      byte_string({0x06, 0x00, 0x04, 0x06, 0x01, 0x05}),
      byte_string({0x07, 0x00, 0x06, 0x07, 0x01, 0x07}),
      byte_string({0x07, 0x00, 0x04, 0x07, 0x01, 0x05}),
      byte_string({0x08, 0x0d, 0x0a}),
      byte_string({0x08, 0x06, 0x00}),
      byte_string({0x08, 0x07, 0x05}),
      byte_string({0x06, 0x00, 0x00, 0x07, 0x01, 0x0d, 0x07, 0x01, 0x0e}),
      byte_string({0x06, 0x01, 0x04, 0x06, 0x01, 0x05, 0x07, 0x00, 0x0d}),
      byte_string({0x06, 0x00, 0x06, 0x07, 0x01, 0x0f, 0x06, 0x01, 0x07}),
      byte_string({0x09, 0x00, 0x0d}),
      byte_string({0x09, 0x03, 0x06}),
      byte_string({0x0a, 0x01, 0x0d}),
      byte_string({0x0a, 0x02, 0x04}),
      byte_string({0x0b, 0x01, 0x07}),
      byte_string({0x0b, 0x02, 0x05}),
      byte_string({0x0c, 0x00, 0x0d}),
      byte_string({0x0c, 0x03, 0x06}),
      byte_string({0x0d, 0x00, 0x15}),
      byte_string({0x0d, 0x04, 0x1b}),
      byte_string({0x0e, 0x02, 0x10}),
      byte_string({0x0e, 0x05, 0x1d}),
      byte_string({0x09, 0x00, 0x04, 0x0a, 0x01, 0x0d}),
      byte_string({0x0b, 0x02, 0x07, 0x09, 0x01, 0x06}),
      byte_string({0x0c, 0x01, 0x04, 0x0b, 0x02, 0x07}),
      byte_string({0x0c, 0x00, 0x0d, 0x0a, 0x01, 0x06}),
      byte_string({0x0d, 0x01, 0x18, 0x0c, 0x03, 0x06}),
      byte_string({0x0e, 0x03, 0x12, 0x0a, 0x02, 0x04}),
      byte_string({0x0e, 0x00, 0x15, 0x0d, 0x04, 0x1b}),
      byte_string({0x0d, 0x02, 0x1f, 0x0e, 0x03, 0x20, 0x0c, 0x01, 0x07}),
      byte_string({0x0e, 0x04, 0x24, 0x0d, 0x01, 0x19, 0x0b, 0x02, 0x05}),
      byte_string({0x0c, 0x00, 0x10, 0x0d, 0x03, 0x21, 0x0e, 0x02, 0x18}),
      byte_string({0x07, 0x01, 0x10, 0x08, 0x1f, 0x20, 0x0d, 0x02, 0x22}),
  };
}

void StressLanguageSingleDocumentPipeline(int seedIndex,
                                          const std::string &mutationProgram) {
  const auto &scenarios = stress_single_document_scenarios();
  ASSERT_GE(seedIndex, 0);
  ASSERT_LT(static_cast<std::size_t>(seedIndex), scenarios.size());
  expect_workspace_round_trip(scenarios[static_cast<std::size_t>(seedIndex)], 0u,
                              mutationProgram);
}

void StressLanguageArbitraryDocumentBuild(const std::string &source) {
  expect_stress_document_build(source);
}

void AdversarialLanguageSingleDocumentPipeline(
    int seedIndex, const std::string &mutationProgram) {
  const auto &scenarios = adversarial_single_document_scenarios();
  ASSERT_GE(seedIndex, 0);
  ASSERT_LT(static_cast<std::size_t>(seedIndex), scenarios.size());
  expect_workspace_round_trip(scenarios[static_cast<std::size_t>(seedIndex)], 0u,
                              mutationProgram);
}

void AdversarialLanguageArbitraryDocumentBuild(const std::string &source) {
  expect_adversarial_document_build(source);
}

void AdversarialWorkspaceRelinkAndRecovery(int scenarioIndex, int targetIndex,
                                           const std::string &mutationProgram) {
  const auto &scenarios = adversarial_workspace_scenarios();
  ASSERT_GE(scenarioIndex, 0);
  ASSERT_LT(static_cast<std::size_t>(scenarioIndex), scenarios.size());
  const auto &scenario = scenarios[static_cast<std::size_t>(scenarioIndex)];
  ASSERT_GE(targetIndex, 0);
  const auto normalizedTargetIndex =
      static_cast<std::size_t>(targetIndex) % scenario.documents.size();
  expect_workspace_round_trip(scenario, normalizedTargetIndex, mutationProgram);
}

[[nodiscard]] int adversarial_workspace_target_count() {
  const auto &scenarios = adversarial_workspace_scenarios();
  const auto maxDocuments = std::ranges::max(
      scenarios | std::views::transform([](const WorkspaceScenarioSpec &scenario) {
        return scenario.documents.size();
      }));
  return static_cast<int>(maxDocuments);
}

} // namespace

TEST(PegiumWorkspaceRegressionTest, TruncatedEofLegacyIncrementalUpdate) {
  const auto &scenarios = stress_single_document_scenarios();
  const auto it = std::ranges::find_if(
      scenarios, [](const WorkspaceScenarioSpec &scenario) {
        return scenario.name == "stress/truncated-eof.stress";
      });
  ASSERT_NE(it, scenarios.end());

  expect_workspace_round_trip(
      *it, 0u,
      byte_string({0x00, 0x00, 0x00, 0x00, 0x01, 0x07, 0x00, 0x33, 0x00, 0x00,
                   0x00, 0x06, 0x00, 0x11, 0x04, 0x23, 0x4c, 0x65, 0x3a, 0x00}));
}

TEST(PegiumWorkspaceRegressionTest, AdversarialCoverageIncrementalRoundTrip) {
  const auto &scenarios = adversarial_single_document_scenarios();
  const auto it = std::ranges::find_if(
      scenarios, [](const WorkspaceScenarioSpec &scenario) {
        return scenario.name == "adversarial/coverage.adv";
      });
  ASSERT_NE(it, scenarios.end());
  expect_workspace_round_trip(*it, 0u, std::string{});
}

TEST(PegiumWorkspaceRegressionTest, AdversarialWorkspaceRelinkRoundTrip) {
  const auto &scenarios = adversarial_workspace_scenarios();
  const auto it = std::ranges::find_if(
      scenarios, [](const WorkspaceScenarioSpec &scenario) {
        return scenario.name == "adversarial/workspace-relink-basic";
      });
  ASSERT_NE(it, scenarios.end());
  expect_workspace_round_trip(*it, 0u, std::string{});
  expect_workspace_round_trip(*it, 1u, std::string{});
}

TEST(PegiumWorkspaceRegressionTest, AdversarialThreeHopWorkspaceRelinkRoundTrip) {
  const auto &scenarios = adversarial_workspace_scenarios();
  const auto it = std::ranges::find_if(
      scenarios, [](const WorkspaceScenarioSpec &scenario) {
        return scenario.name == "adversarial/workspace-relink-three-hop";
      });
  ASSERT_NE(it, scenarios.end());
  expect_workspace_round_trip(*it, 0u, std::string{});
  expect_workspace_round_trip(*it, 1u, std::string{});
  expect_workspace_round_trip(*it, 2u, std::string{});
}

TEST(PegiumWorkspaceRegressionTest, AdversarialFanoutWorkspaceRelinkRoundTrip) {
  const auto &scenarios = adversarial_workspace_scenarios();
  const auto it = std::ranges::find_if(
      scenarios, [](const WorkspaceScenarioSpec &scenario) {
        return scenario.name == "adversarial/workspace-relink-fanout";
      });
  ASSERT_NE(it, scenarios.end());
  expect_workspace_round_trip(*it, 0u, std::string{});
  expect_workspace_round_trip(*it, 1u, std::string{});
  expect_workspace_round_trip(*it, 2u, std::string{});
  expect_workspace_round_trip(*it, 3u, std::string{});
}

FUZZ_TEST(PegiumWorkspaceFuzzTest, StressLanguageSingleDocumentPipeline)
    .WithDomains(
        fuzztest::InRange(
            0, static_cast<int>(stress_single_document_scenarios().size() - 1u)),
        fuzztest::Arbitrary<std::string>().WithMaxSize(64))
    .WithSeeds([]() -> std::vector<std::tuple<int, std::string>> {
      std::vector<std::tuple<int, std::string>> seeds;
      const auto &scenarios = stress_single_document_scenarios();
      const auto mutations = mutation_programs();
      for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size();
           ++scenarioIndex) {
        for (const auto &mutation : mutations) {
          seeds.emplace_back(static_cast<int>(scenarioIndex), mutation);
        }
      }
      return seeds;
    });

FUZZ_TEST(PegiumWorkspaceFuzzTest, StressLanguageArbitraryDocumentBuild)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMaxSize(4096))
    .WithSeeds([]() -> std::vector<std::tuple<std::string>> {
      std::vector<std::tuple<std::string>> seeds{
          std::tuple{std::string{}},
          std::tuple{std::string{"module\n"}},
          std::tuple{std::string{"module X\nexpr 1 + ;\n"}},
          std::tuple{std::string{"module X\nexpr (1 + 2\nlegacy 4 *\n"}},
          std::tuple{std::string{"module X\nlegacy (1 + );\n"}},
          std::tuple{std::string{"module X\nlegacy 4 *\n"}},
          std::tuple{std::string{"module X\nchoose ;\n"}},
          std::tuple{std::string{"module X\nchoose \n"}},
          std::tuple{std::string{"module X\nuse A B;\n"}},
          std::tuple{std::string{"module X\nuse A, B fallback\n"}},
          std::tuple{std::string{"module X\nbag alpha gamma;\n"}},
          std::tuple{std::string{"module X\ntuple(one,,);\n"}},
          std::tuple{std::string{"module X\ntuple(one two, three);\n"}},
          std::tuple{std::string{"module X\ntuple(\n one,\n two);\n"}},
          std::tuple{std::string{"module X\ntuple(one, /* gap */ two);\n"}},
          std::tuple{std::string{"module X\ndoc 'unterminated\n"}},
          std::tuple{std::string{"module X\ndoc \"slash at eof \\\n"}},
          std::tuple{
              std::string{"module X\nexpr Ref(1, Ref(2, /* unterminated\n"}},
          std::tuple{std::string{"module X\n/* unterminated"}},
          std::tuple{std::string{"module X\nlegacy +;\n"}},
          std::tuple{std::string{"module X\nlegacy a + + b;\n"}},
          std::tuple{std::string{"module X\nlegacy 1 * / 2;\n"}},
          std::tuple{std::string{"module X\nlegacy a b;\n"}},
          std::tuple{
              std::string{"module X\nexpr Ref(1 /* gap */, Ref(2, 3 + ));\n"}},
          std::tuple{
              std::string{"module X\nlegacy 1 /* gap */ + /* hole */\n"}},
          std::tuple{std::string{"module X\nbag alpha gamma alpha;\n"}},
          std::tuple{std::string{"module X\npeek alpha beta;\n"}},
          std::tuple{std::string{"module X\nguard token=;\n"}},
          std::tuple{std::string{"module X\npath one./*gap*/two;\n"}},
          std::tuple{std::string{"module X\nuse A., B fallback .C;\n"}},
          std::tuple{
              std::string{"module X\ndecl Base extends {\n  self Base;\n"}},
          std::tuple{
              std::string{"module X\ndecl C extends Base/* gap */.Inner {\n"}},
          std::tuple{
              std::string{"module X\nexpr Ref(Ref(1, 2), Ref(3, (4 + 5)));\n"}},
          std::tuple{
              std::string{"module X\nexpr Ref(1, Ref(2, /* hole */ 3 + ));\n"}},
          std::tuple{
              std::string{"module X\nlegacy Ref + Ref - Ref * Ref + ;\n"}},
          std::tuple{
              std::string{"module X\nuse Base, Child fallback Base\npath Base.Child..Tail;\n"}},
          std::tuple{
              std::string{"module X\ndoc \"quote \\\" slash \\\\ tail\";\n/* unterminated"}},
          std::tuple{std::string{"module X\nsetting safe =\n"}},
          std::tuple{std::string{"/*"}},
          std::tuple{std::string{"doc \""}},
          std::tuple{std::string{"expr ;\n"}},
          std::tuple{std::string("\0", 1)},
      };
      for (const auto &scenario : stress_single_document_scenarios()) {
        seeds.emplace_back(scenario.documents.front().text);
      }
      return seeds;
    });

FUZZ_TEST(PegiumWorkspaceFuzzTest, AdversarialLanguageSingleDocumentPipeline)
    .WithDomains(
        fuzztest::InRange(
            0, static_cast<int>(adversarial_single_document_scenarios().size() - 1u)),
        fuzztest::Arbitrary<std::string>().WithMaxSize(64))
    .WithSeeds([]() -> std::vector<std::tuple<int, std::string>> {
      std::vector<std::tuple<int, std::string>> seeds;
      const auto &scenarios = adversarial_single_document_scenarios();
      const auto mutations = mutation_programs();
      for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size();
           ++scenarioIndex) {
        for (const auto &mutation : mutations) {
          seeds.emplace_back(static_cast<int>(scenarioIndex), mutation);
        }
      }
      return seeds;
    });

FUZZ_TEST(PegiumWorkspaceFuzzTest, AdversarialLanguageArbitraryDocumentBuild)
    .WithDomains(fuzztest::Arbitrary<std::string>().WithMaxSize(4096))
    .WithSeeds([]() -> std::vector<std::tuple<std::string>> {
      std::vector<std::tuple<std::string>> seeds{
          std::tuple{std::string{}},
          std::tuple{std::string{"graph\n"}},
          std::tuple{std::string{"graph X\nnode Root {\n  self: Root = Root;\n}\n"}},
          std::tuple{std::string{"graph X\neval Root<Root>(Root\n"}},
          std::tuple{std::string{"graph X\nlink Root -> when ;\n"}},
          std::tuple{std::string{"graph X\nmix hot warm;\nprobe fast slow;\n"}},
          std::tuple{std::string{"graph X\npack[one two,,];\n"}},
          std::tuple{std::string{"graph X\neval {key Root, tail: [1, 2]};\n"}},
          std::tuple{std::string{"graph X\nlegacy Root && * Leaf;\n"}},
          std::tuple{std::string{"graph X\nalias Main = Root::;\n"}},
          std::tuple{
              std::string{"graph X\ncase outer when Root {\n  eval Root<Root>(Root);\n"}},
          std::tuple{
              std::string{"graph X\neval Root<Root>(Root, {tail: [Root, /*"}},
          std::tuple{
              std::string{"graph X\nnode Root {\n  self: Root = {tail: [Root, Root<Root>(Root)]\n"}},
          std::tuple{
              std::string{"graph X\ncase outer when Root {\n  link Root -> Root when Root<Root>(Root;\n"}},
          std::tuple{
              std::string{"graph X\nexport node Root extends Root when Root && {\n  many child Root = ;\n"}},
          std::tuple{
              std::string{"graph X\neval Root::Inner<Root::Inner>(Root::Inner<Root>(Root), {a: [Root, /* gap */ });\n"}},
      };
      for (const auto &scenario : adversarial_single_document_scenarios()) {
        seeds.emplace_back(scenario.documents.front().text);
      }
      return seeds;
    });

FUZZ_TEST(PegiumWorkspaceFuzzTest, AdversarialWorkspaceRelinkAndRecovery)
    .WithDomains(
        fuzztest::InRange(
            0, static_cast<int>(adversarial_workspace_scenarios().size() - 1u)),
        fuzztest::InRange(0, adversarial_workspace_target_count() - 1),
        fuzztest::Arbitrary<std::string>().WithMaxSize(64))
    .WithSeeds([]() -> std::vector<std::tuple<int, int, std::string>> {
      std::vector<std::tuple<int, int, std::string>> seeds;
      const auto &scenarios = adversarial_workspace_scenarios();
      const auto mutations = mutation_programs();
      for (std::size_t scenarioIndex = 0; scenarioIndex < scenarios.size();
           ++scenarioIndex) {
        for (std::size_t targetIndex = 0;
             targetIndex < scenarios[scenarioIndex].documents.size();
             ++targetIndex) {
          for (const auto &mutation : mutations) {
            seeds.emplace_back(static_cast<int>(scenarioIndex),
                               static_cast<int>(targetIndex), mutation);
          }
        }
      }
      return seeds;
    });

} // namespace pegium::fuzz
