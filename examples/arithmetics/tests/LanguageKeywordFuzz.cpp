#include <gtest/gtest.h>

#include <arithmetics/core/CoreModule.hpp>

#include <pegium/examples/KeywordFuzzHarness.hpp>

#include <array>
#include <memory>
#include <string_view>

namespace arithmetics::tests {
namespace {

namespace fuzz = pegium::test::keyword_fuzz;

constexpr std::string_view kBaseText =
    "module fuzzMath\n"
    "\n"
    "def a: 5;\n"
    "def b: 3;\n"
    "def c: a + b;\n"
    "def d: (a ^ b);\n"
    "\n"
    "def root(x, y):\n"
    "    x ^ (1 / y);\n"
    "\n"
    "def sqrt(x):\n"
    "    root(x, 2);\n"
    "\n"
    "2 * c;\n"
    "b % 2;\n"
    "root(d, 3);\n";

constexpr std::string_view kDroppableSymbols = "(),";
constexpr std::array<std::string_view, 2> kKeywords = {"module", "def"};

constexpr const char *kEnvVar = "PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE";
constexpr std::string_view kFileSuffix = ".calc";
constexpr std::string_view kLanguageId = "arithmetics";

struct ParserFactory {
  std::unique_ptr<const pegium::parser::Parser> parser =
      createArithmeticsParser();
  const pegium::parser::Parser &operator()() const noexcept { return *parser; }
};

TEST(ArithmeticsKeywordFuzzTest, SingleMutationAlwaysRecovers) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_single_sweep(factory, kBaseText, kDroppableSymbols, kKeywords,
                          kFileSuffix, kLanguageId);
}

TEST(ArithmeticsKeywordFuzzTest, PairMutationsAlwaysRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_pair_sweep(factory, kBaseText, kDroppableSymbols, kKeywords,
                        kFileSuffix, kLanguageId);
}

TEST(ArithmeticsKeywordFuzzTest, RandomTriples) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_random_triples_sweep(factory, kBaseText, kDroppableSymbols,
                                   kKeywords, kFileSuffix, kLanguageId,
                                   /*seed=*/0xA217CUL, /*iterations=*/200);
}

TEST(ArithmeticsKeywordFuzzTest, RandomLargerMutationsRecover) {
  if (!fuzz::fuzz_sweeps_enabled(kEnvVar)) {
    GTEST_SKIP() << "Set " << kEnvVar << "=1 to run.";
  }
  ParserFactory factory;
  fuzz::run_random_larger_sweep(factory, kBaseText, kDroppableSymbols,
                                  kKeywords, kFileSuffix, kLanguageId,
                                  /*seed=*/0xA21B16ULL, /*iterations=*/150);
}

} // namespace
} // namespace arithmetics::tests
