#include <gtest/gtest.h>

#include <statemachine/core/Parser.hpp>

#include <pegium/examples/KeywordFuzzHarness.hpp>

#include <array>
#include <string_view>

namespace statemachine::tests {
namespace {

namespace fuzz = pegium::test::keyword_fuzz;

constexpr std::string_view kBaseText =
    "statemachine traffic_light\n"
    "events\n"
    "    timer_tick\n"
    "    pedestrian_button\n"
    "commands\n"
    "    show_red\n"
    "    show_green\n"
    "    show_yellow\n"
    "initialState red\n"
    "\n"
    "state red\n"
    "    actions { show_red }\n"
    "    timer_tick => green\n"
    "end\n"
    "\n"
    "state green\n"
    "    actions { show_green }\n"
    "    timer_tick => yellow\n"
    "    pedestrian_button => yellow\n"
    "end\n"
    "\n"
    "state yellow\n"
    "    actions { show_yellow }\n"
    "    timer_tick => red\n"
    "end\n";

constexpr std::string_view kDroppableSymbols = "{}";
constexpr std::array<std::string_view, 7> kKeywords = {
    "statemachine", "events",       "commands", "initialState",
    "state",        "actions",      "end"};

constexpr const char *kEnvVar = "PEGIUM_STATEMACHINE_FUZZ_EXHAUSTIVE";
constexpr std::string_view kFileSuffix = ".statemachine";
constexpr std::string_view kLanguageId = "statemachine";

struct ParserFactory {
  parser::StateMachineParser parser;
  parser::StateMachineParser &operator()() noexcept { return parser; }
};

TEST(StatemachineKeywordFuzzTest, SingleMutationAlwaysRecovers) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_single_sweep(factory, kBaseText, kDroppableSymbols, kKeywords,
                          kFileSuffix, kLanguageId);
}

TEST(StatemachineKeywordFuzzTest, PairMutationsAlwaysRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_pair_sweep(factory, kBaseText, kDroppableSymbols, kKeywords,
                        kFileSuffix, kLanguageId);
}

TEST(StatemachineKeywordFuzzTest, RandomTriples) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_random_triples_sweep(factory, kBaseText, kDroppableSymbols,
                                   kKeywords, kFileSuffix, kLanguageId,
                                   /*seed=*/0x57A7E3CULL,
                                   /*iterations=*/200);
}

TEST(StatemachineKeywordFuzzTest, RandomLargerMutationsRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_random_larger_sweep(factory, kBaseText, kDroppableSymbols,
                                  kKeywords, kFileSuffix, kLanguageId,
                                  /*seed=*/0x57A7B16ULL,
                                  /*iterations=*/150);
}

} // namespace
} // namespace statemachine::tests
