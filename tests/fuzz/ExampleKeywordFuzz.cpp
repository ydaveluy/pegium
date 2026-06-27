// Coverage-guided (dynamic) keyword/symbol fuzz for the example languages.
//
// The static counterparts in `examples/<lang>/tests/LanguageKeywordFuzz.cpp`
// drive a deterministic sweep over (single, pair, random-triple, larger)
// combinations of two primitive mutations:
//   - DropChar      : delete one byte from the base text
//   - TruncateKeyword: delete the trailing byte of a keyword occurrence
// They are gated on `PEGIUM_*_FUZZ_EXHAUSTIVE=1`, run as plain GTests, and
// terminate after a fixed iteration count.
//
// This file registers FuzzTest-driven targets that run the SAME mutation
// model under coverage-guided mutation: each input is a small list of
// `(kind, index)` pairs, the framework explores the space and persists
// crash/finding inputs across runs. The base text + droppable symbols +
// keyword list per language mirror the static harness exactly, so a
// finding here can be replayed in the static suite trivially.
//
// Each language gets one FUZZ_TEST. They share the same template helper
// `expect_keyword_fuzz_recovery` to keep the per-language code small.

#include <fuzztest/fuzztest.h>
#include <gtest/gtest.h>

#include <pegium/examples/KeywordFuzzHarness.hpp>

#include <arithmetics/core/CoreModule.hpp>
#include <domainmodel/core/CoreModule.hpp>
#include <requirements/core/CoreModule.hpp>
#include <statemachine/core/CoreModule.hpp>

#include <pegium/core/text/TextSnapshot.hpp>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::fuzz {
namespace {

namespace kf = pegium::test::keyword_fuzz;

// ---------- Per-language descriptors -----------------------------------

struct LanguageDescriptor {
  std::string_view name;
  std::string_view baseText;
  std::string_view droppableSymbols;
  std::span<const std::string_view> keywords;
  std::string_view fileSuffix;
  std::string_view languageId;
};

constexpr std::string_view kArithmeticsBaseText =
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

constexpr std::array<std::string_view, 2> kArithmeticsKeywords = {
    "module", "def"};

constexpr std::string_view kDomainModelBaseText =
    "datatype String\n"
    "datatype Int\n"
    "\n"
    "package person {\n"
    "  entity Person {\n"
    "    name: String\n"
    "    age: Int\n"
    "  }\n"
    "}\n"
    "\n"
    "package company {\n"
    "  entity Company extends person.Person {\n"
    "    employees: many Person\n"
    "  }\n"
    "}\n";

constexpr std::array<std::string_view, 5> kDomainModelKeywords = {
    "datatype", "package", "entity", "extends", "many"};

constexpr std::string_view kStateMachineBaseText =
    "statemachine Demo\n"
    "\n"
    "events e1 e2 e3\n"
    "\n"
    "state Idle\n"
    "  e1 => Active\n"
    "end\n"
    "\n"
    "state Active\n"
    "  e2 => Done\n"
    "  e3 => Idle\n"
    "end\n";

constexpr std::array<std::string_view, 4> kStateMachineKeywords = {
    "statemachine", "events", "state", "end"};

constexpr std::string_view kRequirementsBaseText =
    "requirement R1: \"Initial requirement\"\n"
    "  priority: high\n"
    "  status: open\n"
    "end\n"
    "\n"
    "requirement R2: \"Depends on R1\"\n"
    "  refines: R1\n"
    "end\n";

constexpr std::array<std::string_view, 4> kRequirementsKeywords = {
    "requirement", "priority", "status", "refines"};

// ---------- Generic round-trip helper ----------------------------------

// One fuzz iteration applies up to `kMaxMutationsPerInput` (kind,index)
// pairs from the input vector to the base text and parses the result.
// The harness asserts whatever invariants the language's parser exposes
// (ASan/UBSan catch UB, and FuzzTest catches uncaught exceptions and
// excessive runtime). The expected behaviour is "parse must terminate
// and either succeed or fail gracefully" — recovery may or may not
// produce a credible AST depending on which keywords/symbols were
// damaged.
constexpr std::size_t kMaxMutationsPerInput = 6u;

template <typename ParserType>
void expect_keyword_fuzz_recovery(
    ParserType &parser, const LanguageDescriptor &lang,
    const std::vector<std::pair<int, int>> &program) {
  SCOPED_TRACE(lang.name);

  // Collect candidate mutation positions from the base text.
  // (Same predicate as the static harness, see KeywordFuzzHarness.hpp.)
  const auto candidates =
      kf::collect_mutations(lang.baseText, lang.droppableSymbols, lang.keywords);
  if (candidates.empty()) {
    return;
  }

  // Map fuzz input pairs to mutations. We bound the count + dedup
  // overlapping positions in `apply_mutations`.
  std::vector<kf::Mutation> mutations;
  mutations.reserve(std::min(program.size(), kMaxMutationsPerInput));
  for (const auto &[rawKind, rawIndex] : program) {
    if (mutations.size() >= kMaxMutationsPerInput) {
      break;
    }
    const auto candidateIndex =
        static_cast<std::size_t>(static_cast<unsigned>(rawIndex)) %
        candidates.size();
    auto chosen = candidates[candidateIndex];
    // The fuzz `kind` parameter selects whether we keep the natural kind
    // for that position or coerce it to the "drop one byte" primitive.
    if ((rawKind & 1) != 0) {
      chosen.kind = kf::Mutation::Kind::DropChar;
      chosen.length = 1u;
    }
    mutations.push_back(chosen);
  }
  if (mutations.empty()) {
    return;
  }

  const auto mutated =
      kf::apply_mutations(lang.baseText, std::move(mutations));
  if (mutated.empty()) {
    return; // overlapping mutations were rejected — skip silently.
  }

  // Parse with recovery enabled (default). FuzzTest will catch any
  // exception, sanitizer report, or hang; we don't assert specific
  // recovery outcomes here because the interesting failure modes
  // (crashes, leaks, UB, deadlocks) are detected by the runtime.
  try {
    // The public `parse(std::string_view)` overload copies the input into
    // a TextSnapshot internally.
    auto result = parser.parse(std::string_view{mutated});
    // Touch the result to keep optimisers honest.
    (void)result.recoveryReport.recoveryCount;
    (void)result.parseDiagnostics.size();
    (void)result.fullMatch;
    (void)result.parsedLength;
  } catch (const std::exception &) {
    // The parser must not throw on recoverable input. Re-throwing is
    // what FuzzTest needs to flag a finding.
    throw;
  }
}

// ---------- FUZZ_TEST entry points -------------------------------------

void ArithmeticsKeywordFuzz(
    const std::vector<std::pair<int, int>> &program) {
  auto parser = arithmetics::createArithmeticsParser();
  expect_keyword_fuzz_recovery(*parser,
                                LanguageDescriptor{
                                    .name = "arithmetics",
                                    .baseText = kArithmeticsBaseText,
                                    .droppableSymbols = "(),",
                                    .keywords = kArithmeticsKeywords,
                                    .fileSuffix = ".calc",
                                    .languageId = "arithmetics",
                                },
                                program);
}

void DomainModelKeywordFuzz(
    const std::vector<std::pair<int, int>> &program) {
  auto parser = domainmodel::createDomainModelParser();
  expect_keyword_fuzz_recovery(*parser,
                                LanguageDescriptor{
                                    .name = "domainmodel",
                                    .baseText = kDomainModelBaseText,
                                    .droppableSymbols = "{},.:",
                                    .keywords = kDomainModelKeywords,
                                    .fileSuffix = ".dmodel",
                                    .languageId = "domainmodel",
                                },
                                program);
}

void StateMachineKeywordFuzz(
    const std::vector<std::pair<int, int>> &program) {
  auto parser = statemachine::createStatemachineParser();
  expect_keyword_fuzz_recovery(*parser,
                                LanguageDescriptor{
                                    .name = "statemachine",
                                    .baseText = kStateMachineBaseText,
                                    .droppableSymbols = "=>",
                                    .keywords = kStateMachineKeywords,
                                    .fileSuffix = ".sm",
                                    .languageId = "statemachine",
                                },
                                program);
}

void RequirementsKeywordFuzz(
    const std::vector<std::pair<int, int>> &program) {
  auto parser = requirements::createRequirementsParser();
  expect_keyword_fuzz_recovery(*parser,
                                LanguageDescriptor{
                                    .name = "requirements",
                                    .baseText = kRequirementsBaseText,
                                    .droppableSymbols = ":,\"",
                                    .keywords = kRequirementsKeywords,
                                    .fileSuffix = ".req",
                                    .languageId = "requirements",
                                },
                                program);
}

// FuzzTest domain shared by all four FUZZ_TESTs.
//   - First int (kind)  : 0 = natural mutation kind for the chosen
//                          position, 1 = coerce to DropChar.
//   - Second int (index): selects a candidate mutation position
//                          modulo `collect_mutations(...).size()`.
auto keyword_fuzz_domain() {
  return fuzztest::VectorOf(fuzztest::PairOf(
                                fuzztest::InRange(0, 1),
                                fuzztest::InRange(0, 1023)))
      .WithMinSize(1)
      .WithMaxSize(kMaxMutationsPerInput);
}

} // namespace

FUZZ_TEST(PegiumExampleKeywordFuzzTest, ArithmeticsKeywordFuzz)
    .WithDomains(keyword_fuzz_domain());

FUZZ_TEST(PegiumExampleKeywordFuzzTest, DomainModelKeywordFuzz)
    .WithDomains(keyword_fuzz_domain());

FUZZ_TEST(PegiumExampleKeywordFuzzTest, StateMachineKeywordFuzz)
    .WithDomains(keyword_fuzz_domain());

FUZZ_TEST(PegiumExampleKeywordFuzzTest, RequirementsKeywordFuzz)
    .WithDomains(keyword_fuzz_domain());

} // namespace pegium::fuzz
