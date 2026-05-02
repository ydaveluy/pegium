#include <gtest/gtest.h>

#include <domainmodel/parser/Parser.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <functional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace domainmodel::tests {
namespace {

/// Domain-model text exercising every keyword (`package`, `datatype`,
/// `entity`, `extends`, `many`) and every structural symbol (`:`, `{`,
/// `}`, `.`). The fuzz harness mutates this base by deleting symbols or
/// truncating keywords; the parser is expected to recover every variant.
constexpr std::string_view kKeywordFuzzBaseText =
    "package blog.core {\n"
    "  datatype String\n"
    "  datatype Date\n"
    "\n"
    "  entity Person {\n"
    "    name: String\n"
    "    birth: blog.core.Date\n"
    "  }\n"
    "\n"
    "  entity Author extends Person {\n"
    "    pseudonym: String\n"
    "  }\n"
    "\n"
    "  entity Post {\n"
    "    title: String\n"
    "    author: Author\n"
    "    many tags: String\n"
    "  }\n"
    "\n"
    "  entity Comment extends Person {\n"
    "    body: String\n"
    "    parent: Post\n"
    "  }\n"
    "}\n";

/// A single mutation step. `kind == DropChar` deletes one byte at
/// `position`; `kind == TruncateKeyword` deletes `length` bytes starting
/// at `position` (used to drop the trailing character of a keyword).
struct Mutation {
  enum class Kind : std::uint8_t { DropChar, TruncateKeyword };

  Kind kind;
  std::size_t position;
  std::size_t length;
  std::string_view label; // human-readable keyword name or symbol

  [[nodiscard]] bool operator<(const Mutation &other) const noexcept {
    return position < other.position;
  }
};

/// All mutations the fuzz sweep can apply: every `:`, `{`, `}`, `.`
/// position becomes a `DropChar`, and every keyword occurrence becomes a
/// `TruncateKeyword` that drops the keyword's last character (the
/// canonical "missing-codepoint" typo class the recovery is designed
/// to repair).
std::vector<Mutation> all_mutations() {
  std::vector<Mutation> mutations;
  for (std::size_t i = 0; i < kKeywordFuzzBaseText.size(); ++i) {
    const auto c = kKeywordFuzzBaseText[i];
    if (c == ':' || c == '{' || c == '}' || c == '.') {
      mutations.push_back({Mutation::Kind::DropChar, i, 1, std::string_view{
                                                              &kKeywordFuzzBaseText[i],
                                                              1}});
    }
  }
  for (const auto keyword : {std::string_view{"package"},
                             std::string_view{"datatype"},
                             std::string_view{"entity"},
                             std::string_view{"extends"},
                             std::string_view{"many"}}) {
    std::size_t cursor = 0;
    while (true) {
      const auto pos = kKeywordFuzzBaseText.find(keyword, cursor);
      if (pos == std::string_view::npos) {
        break;
      }
      // Only treat it as a keyword if surrounded by non-identifier chars
      // (so we don't truncate the substring inside an identifier like
      // `manyfoo` — there is none in the base text, but the guard keeps
      // the harness honest).
      const bool boundedLeft =
          pos == 0 || !(std::isalnum(static_cast<unsigned char>(
                            kKeywordFuzzBaseText[pos - 1])) ||
                        kKeywordFuzzBaseText[pos - 1] == '_');
      const auto endPos = pos + keyword.size();
      const bool boundedRight =
          endPos == kKeywordFuzzBaseText.size() ||
          !(std::isalnum(static_cast<unsigned char>(
                kKeywordFuzzBaseText[endPos])) ||
            kKeywordFuzzBaseText[endPos] == '_');
      if (boundedLeft && boundedRight) {
        // Drop the last character of the keyword; that is the typo class
        // the recovery is required to repair (`entit -> entity`,
        // `extend -> extends`, etc.).
        mutations.push_back({Mutation::Kind::TruncateKeyword,
                             endPos - 1, 1, keyword});
      }
      cursor = endPos;
    }
  }
  return mutations;
}

std::string apply_mutations(std::vector<Mutation> mutations) {
  std::sort(mutations.begin(), mutations.end());
  // Reject overlapping mutations (the random sweeps may pick keyword
  // positions that overlap each other or a symbol drop). Returning the
  // empty string skips the case; callers handle that by treating it as a
  // no-op.
  for (std::size_t i = 1; i < mutations.size(); ++i) {
    const auto prevEnd = mutations[i - 1].position + mutations[i - 1].length;
    if (mutations[i].position < prevEnd) {
      return {};
    }
  }
  std::string result;
  result.reserve(kKeywordFuzzBaseText.size());
  std::size_t cursor = 0;
  for (const auto &m : mutations) {
    result.append(kKeywordFuzzBaseText, cursor, m.position - cursor);
    cursor = m.position + m.length;
  }
  result.append(kKeywordFuzzBaseText, cursor,
                kKeywordFuzzBaseText.size() - cursor);
  return result;
}

std::string describe_mutations(const std::vector<Mutation> &mutations) {
  std::string out = "mutate{";
  for (std::size_t i = 0; i < mutations.size(); ++i) {
    if (i > 0) {
      out += ',';
    }
    out += std::to_string(mutations[i].position);
    out += '(';
    if (mutations[i].kind == Mutation::Kind::DropChar) {
      out += "drop:";
    } else {
      out += "trunc:";
    }
    out += mutations[i].label;
    out += ')';
  }
  out += '}';
  return out;
}

void expect_recovered(parser::DomainModelParser &parser,
                      const std::vector<Mutation> &mutations) {
  const auto text = apply_mutations(mutations);
  if (text.empty()) {
    return; // skipped: overlapping mutations
  }
  auto document = pegium::test::parse_document(
      parser, text, pegium::test::make_file_uri("keyword-fuzz.dmodel"),
      "domain-model");
  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  const auto label = describe_mutations(mutations);
  EXPECT_TRUE(parsed.value != nullptr)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
  EXPECT_TRUE(parsed.fullMatch)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
}

void enumerate_combinations(
    const std::vector<Mutation> &mutations, std::size_t k,
    const std::function<void(const std::vector<Mutation> &)> &visit) {
  std::vector<Mutation> pick(k);
  std::function<void(std::size_t, std::size_t)> rec =
      [&](std::size_t start, std::size_t depth) {
        if (depth == k) {
          visit(pick);
          return;
        }
        for (std::size_t i = start;
             i + (k - depth) <= mutations.size(); ++i) {
          pick[depth] = mutations[i];
          rec(i + 1, depth + 1);
        }
      };
  rec(0, 0);
}

/// The fuzz sweeps are skipped by default because an exhaustive run is
/// slow in debug builds. A dedicated ctest registration enables
/// PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1 and runs them as a standalone
/// "fuzz-corpus" test; locally, set the env var to reproduce.
bool fuzz_sweeps_enabled() noexcept {
  const char *v = std::getenv("PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE");
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

TEST(DomainModelKeywordFuzzTest, SingleMutationAlwaysRecovers) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::DomainModelParser parser;
  const auto mutations = all_mutations();
  enumerate_combinations(mutations, 1u,
                         [&](const std::vector<Mutation> &pick) {
                           expect_recovered(parser, pick);
                         });
}

TEST(DomainModelKeywordFuzzTest, PairMutationsAlwaysRecover) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::DomainModelParser parser;
  const auto mutations = all_mutations();
  enumerate_combinations(mutations, 2u,
                         [&](const std::vector<Mutation> &pick) {
                           expect_recovered(parser, pick);
                         });
}

TEST(DomainModelKeywordFuzzTest, RandomTriples) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::DomainModelParser parser;
  const auto mutations = all_mutations();

  std::mt19937 rng(0xD0DE1);
  constexpr int kIterations = 200;
  for (int i = 0; i < kIterations; ++i) {
    std::vector<Mutation> shuffled = mutations;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(3);
    expect_recovered(parser, shuffled);
  }
}

TEST(DomainModelKeywordFuzzTest, RandomLargerMutationsRecover) {
  if (!fuzz_sweeps_enabled()) {
    GTEST_SKIP() << "Set PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1 to run.";
  }
  parser::DomainModelParser parser;
  const auto mutations = all_mutations();

  // Deterministic seed so CI failures are reproducible.
  std::mt19937 rng(0xDA7AC);
  constexpr int kIterations = 150;
  for (int i = 0; i < kIterations; ++i) {
    const auto k = std::uniform_int_distribution<std::size_t>(
        3, std::min<std::size_t>(mutations.size(), 8))(rng);
    std::vector<Mutation> shuffled = mutations;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(k);
    expect_recovered(parser, shuffled);
  }
}

} // namespace
} // namespace domainmodel::tests
