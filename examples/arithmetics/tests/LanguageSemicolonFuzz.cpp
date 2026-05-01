#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace arithmetics::tests {
namespace {

/// Module text with multiple definitions/evaluations whose `;` and `:`
/// separators can be randomly dropped to produce syntactically broken but
/// semantically recoverable Arithmetics modules.
constexpr std::string_view kSemicolonFuzzBaseText =
    "Module basicMath\n"
    "\n"
    "def a: 5;\n"
    "def b: 3;\n"
    "def b1: 3;\n"
    "def b2: 3;\n"
    "def c: a + b; // 8\n"
    "def d: (a ^ b); // 164\n"
    "\n"
    "def root(x, y):\n"
    "    x^(1/y);\n"
    "\n"
    "def sqrt(x):\n"
    "    root(x, 2);\n"
    "\n"
    "2 * c; // 16\n"
    "b % 2; // 1\n";

std::vector<std::size_t> deletable_positions() {
  std::vector<std::size_t> positions;
  for (std::size_t i = 0; i < kSemicolonFuzzBaseText.size(); ++i) {
    const auto c = kSemicolonFuzzBaseText[i];
    if (c == ';' || c == ':') {
      positions.push_back(i);
    }
  }
  return positions;
}

std::string apply_deletions(std::vector<std::size_t> sortedDeletions) {
  std::sort(sortedDeletions.begin(), sortedDeletions.end());
  std::string result;
  result.reserve(kSemicolonFuzzBaseText.size());
  std::size_t cursor = 0;
  for (const auto pos : sortedDeletions) {
    result.append(kSemicolonFuzzBaseText, cursor, pos - cursor);
    cursor = pos + 1;
  }
  result.append(kSemicolonFuzzBaseText, cursor,
                kSemicolonFuzzBaseText.size() - cursor);
  return result;
}

std::string describe_deletions(const std::vector<std::size_t> &deletions) {
  std::string out = "drop{";
  for (std::size_t i = 0; i < deletions.size(); ++i) {
    if (i > 0) {
      out += ',';
    }
    out += std::to_string(deletions[i]);
    out += '(';
    out += kSemicolonFuzzBaseText[deletions[i]];
    out += ')';
  }
  out += '}';
  return out;
}

void expect_recovered(parser::ArithmeticParser &parser,
                      const std::vector<std::size_t> &deletions) {
  const auto text = apply_deletions(deletions);
  auto parsed = parser.parse(text);
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  const auto label = describe_deletions(deletions);
  EXPECT_TRUE(parsed.value.get() != nullptr)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
  EXPECT_TRUE(parsed.fullMatch)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
}

void enumerate_combinations(
    const std::vector<std::size_t> &positions, std::size_t k,
    const std::function<void(const std::vector<std::size_t> &)> &visit) {
  std::vector<std::size_t> pick(k);
  std::function<void(std::size_t, std::size_t)> rec =
      [&](std::size_t start, std::size_t depth) {
        if (depth == k) {
          visit(pick);
          return;
        }
        for (std::size_t i = start; i + (k - depth) <= positions.size(); ++i) {
          pick[depth] = positions[i];
          rec(i + 1, depth + 1);
        }
      };
  rec(0, 0);
}

/// The fuzz sweeps are skipped by default because an exhaustive run is slow
/// in debug builds. A dedicated ctest registration in CMakeLists sets
/// PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE=1 and runs them as a standalone
/// "fuzz-corpus" test; locally, set the env var to reproduce.
bool fuzz_sweeps_enabled() noexcept {
  const char *v = std::getenv("PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE");
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

TEST(ArithmeticsSemicolonColonFuzzTest, SingleDropAlwaysRecovers) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::ArithmeticParser parser;
  const auto positions = deletable_positions();
  enumerate_combinations(positions, 1u,
                         [&](const std::vector<std::size_t> &pick) {
                           expect_recovered(parser, pick);
                         });
}

TEST(ArithmeticsSemicolonColonFuzzTest, PairDropAlwaysRecovers) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::ArithmeticParser parser;
  const auto positions = deletable_positions();
  enumerate_combinations(positions, 2u,
                         [&](const std::vector<std::size_t> &pick) {
                           expect_recovered(parser, pick);
                         });
}

TEST(ArithmeticsSemicolonColonFuzzTest, RandomTriples) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::ArithmeticParser parser;
  const auto positions = deletable_positions();

  std::mt19937 rng(0xBAD1C);
  constexpr int kIterations = 150;
  for (int i = 0; i < kIterations; ++i) {
    std::vector<std::size_t> shuffled = positions;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(3);
    expect_recovered(parser, shuffled);
  }
}

TEST(ArithmeticsSemicolonColonFuzzTest, RandomLargerDropsRecover) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_ARITHMETICS_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::ArithmeticParser parser;
  const auto positions = deletable_positions();

  // Deterministic seed so CI failures are reproducible.
  std::mt19937 rng(0x5EC0);
  constexpr int kIterations = 100;
  for (int i = 0; i < kIterations; ++i) {
    const auto k = std::uniform_int_distribution<std::size_t>(
        3, std::min<std::size_t>(positions.size(), 10))(rng);
    std::vector<std::size_t> shuffled = positions;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(k);
    expect_recovered(parser, shuffled);
  }
}

} // namespace
} // namespace arithmetics::tests
