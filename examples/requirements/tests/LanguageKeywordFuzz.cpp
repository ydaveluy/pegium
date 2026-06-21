#include <gtest/gtest.h>

#include <requirements/core/Parser.hpp>

#include <pegium/examples/KeywordFuzzHarness.hpp>

#include <array>
#include <string_view>

namespace requirements::tests {
namespace {

namespace fuzz = pegium::test::keyword_fuzz;

// ----------------------------------------------------------------------------
// RequirementsParser fuzz
// ----------------------------------------------------------------------------

constexpr std::string_view kRequirementsBaseText =
    "contact: \"alice@example.com\"\n"
    "\n"
    "environment prod: \"production environment\"\n"
    "environment staging: \"pre-prod environment\"\n"
    "\n"
    "req login \"User can log in\" applicable for prod, staging\n"
    "req logout \"User can log out\" applicable for prod\n"
    "req browse \"User can browse\" applicable for staging\n";

constexpr std::string_view kRequirementsSymbols = ":,";
constexpr std::array<std::string_view, 5> kRequirementsKeywords = {
    "contact", "environment", "req", "applicable", "for"};

constexpr const char *kReqEnvVar = "PEGIUM_REQUIREMENTS_FUZZ_EXHAUSTIVE";
constexpr std::string_view kReqFileSuffix = ".req";
constexpr std::string_view kReqLanguageId = "requirements";

struct RequirementsFactory {
  parser::RequirementsParser parser;
  parser::RequirementsParser &operator()() noexcept { return parser; }
};

TEST(RequirementsKeywordFuzzTest, SingleMutationAlwaysRecovers) {
  if (!fuzz::fuzz_sweeps_enabled(kReqEnvVar)) {
    GTEST_SKIP() << "Set " << kReqEnvVar << "=1 to run.";
  }
  RequirementsFactory factory;
  fuzz::run_single_sweep(factory, kRequirementsBaseText, kRequirementsSymbols,
                          kRequirementsKeywords, kReqFileSuffix, kReqLanguageId);
}

TEST(RequirementsKeywordFuzzTest, PairMutationsAlwaysRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kReqEnvVar)) {
    GTEST_SKIP() << "Set " << kReqEnvVar << "=1 to run.";
  }
  RequirementsFactory factory;
  fuzz::run_pair_sweep(factory, kRequirementsBaseText, kRequirementsSymbols,
                        kRequirementsKeywords, kReqFileSuffix, kReqLanguageId);
}

TEST(RequirementsKeywordFuzzTest, RandomTriples) {
  if (!fuzz::fuzz_sweeps_enabled(kReqEnvVar)) {
    GTEST_SKIP() << "Set " << kReqEnvVar << "=1 to run.";
  }
  RequirementsFactory factory;
  fuzz::run_random_triples_sweep(factory, kRequirementsBaseText,
                                   kRequirementsSymbols, kRequirementsKeywords,
                                   kReqFileSuffix, kReqLanguageId,
                                   /*seed=*/0x9E03CULL, /*iterations=*/200);
}

TEST(RequirementsKeywordFuzzTest, RandomLargerMutationsRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kReqEnvVar)) {
    GTEST_SKIP() << "Set " << kReqEnvVar << "=1 to run.";
  }
  RequirementsFactory factory;
  fuzz::run_random_larger_sweep(factory, kRequirementsBaseText,
                                  kRequirementsSymbols, kRequirementsKeywords,
                                  kReqFileSuffix, kReqLanguageId,
                                  /*seed=*/0x9E0B16ULL, /*iterations=*/150);
}

// ----------------------------------------------------------------------------
// TestsParser fuzz
// ----------------------------------------------------------------------------

constexpr std::string_view kTestsBaseText =
    "contact: \"alice@example.com\"\n"
    "\n"
    "tst loginTest testfile = \"tests/login.json\" tests login\n"
    "    applicable for prod\n"
    "tst logoutTest tests logout, browse\n"
    "    applicable for prod, staging\n"
    "tst browseTest testfile = \"tests/browse.json\" tests browse\n";

constexpr std::string_view kTestsSymbols = ":=,";
constexpr std::array<std::string_view, 5> kTestsKeywords = {
    "contact", "tst", "testfile", "tests", "applicable"};

constexpr const char *kTstEnvVar = "PEGIUM_REQUIREMENTS_FUZZ_EXHAUSTIVE";
constexpr std::string_view kTstFileSuffix = ".tst";
constexpr std::string_view kTstLanguageId = "tests";

struct TestsFactory {
  parser::TestsParser parser;
  parser::TestsParser &operator()() noexcept { return parser; }
};

TEST(RequirementsTestsKeywordFuzzTest, SingleMutationAlwaysRecovers) {
  if (!fuzz::fuzz_sweeps_enabled(kTstEnvVar)) {
    GTEST_SKIP() << "Set " << kTstEnvVar << "=1 to run.";
  }
  TestsFactory factory;
  fuzz::run_single_sweep(factory, kTestsBaseText, kTestsSymbols,
                          kTestsKeywords, kTstFileSuffix, kTstLanguageId);
}

TEST(RequirementsTestsKeywordFuzzTest, PairMutationsAlwaysRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kTstEnvVar)) {
    GTEST_SKIP() << "Set " << kTstEnvVar << "=1 to run.";
  }
  TestsFactory factory;
  fuzz::run_pair_sweep(factory, kTestsBaseText, kTestsSymbols, kTestsKeywords,
                        kTstFileSuffix, kTstLanguageId);
}

TEST(RequirementsTestsKeywordFuzzTest, RandomTriples) {
  if (!fuzz::fuzz_sweeps_enabled(kTstEnvVar)) {
    GTEST_SKIP() << "Set " << kTstEnvVar << "=1 to run.";
  }
  TestsFactory factory;
  fuzz::run_random_triples_sweep(factory, kTestsBaseText, kTestsSymbols,
                                   kTestsKeywords, kTstFileSuffix,
                                   kTstLanguageId,
                                   /*seed=*/0x757571CUL, /*iterations=*/200);
}

TEST(RequirementsTestsKeywordFuzzTest, RandomLargerMutationsRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kTstEnvVar)) {
    GTEST_SKIP() << "Set " << kTstEnvVar << "=1 to run.";
  }
  TestsFactory factory;
  fuzz::run_random_larger_sweep(factory, kTestsBaseText, kTestsSymbols,
                                  kTestsKeywords, kTstFileSuffix,
                                  kTstLanguageId,
                                  /*seed=*/0x757B16UL, /*iterations=*/150);
}

} // namespace
} // namespace requirements::tests
