#include <gtest/gtest.h>

#include <arithmetics/core/CoreModule.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/core/parser/ParseDiagnostics.hpp>

#include <string>
#include <string_view>

namespace arithmetics::tests {
namespace {

/// Operator-doubling regression corpus. Pegium recovery used to commit multi-edit
/// speculative paths for inputs that combine a doubled binary operator
/// (`*+`, `-+`, `/+`, `*-`, …) with a missing terminator. The recovery
/// would, for instance, fabricate a `FunctionCall` around the surrounding
/// tokens — `2 * c-4+;` was rewritten as `2 * c(,4);` with five edits.
/// A single `delete +` reaches `fullMatch` with cost 4 and is what the
/// shared `RecoveryKey` selects once the multi-strategy probe
/// surfaces it. These tests pin the behaviour so the symptom
/// cannot regress silently.
///
/// Each test parses a syntactically broken input, asserts that the parser
/// reaches `fullMatch` (the recovery succeeds), and bounds the edit count
/// so that a regression to the old multi-edit speculative path would
/// fail the assertion. A textual oracle would over-constrain the recovery
/// shape; the bound is intentionally loose.

void expect_recovers_within(const std::string &text, std::size_t maxEdits) {
  auto parser = createArithmeticsParser();
  auto parsed = parser->parse(text);
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_LE(parsed.parseDiagnostics.size(), maxEdits)
      << "edit count regressed beyond the expected bound\n"
      << parseDump;
  // Pin the attempt-run count below the global budget. Operator-doubling
  // inputs should resolve in a handful of attempts; a regression that
  // starts exhausting the 1024-attempt budget will trip this bound.
  EXPECT_LT(parsed.recoveryReport.recoveryAttemptRuns, 256u)
      << "recoveryAttemptRuns regressed\n" << parseDump;
}

struct OperatorRecoveryCase {
  const char *name;
  std::string_view text;
  std::size_t maxEdits;
};

static constexpr OperatorRecoveryCase kOperatorRecoveryCases[] = {
    // `2 * c-4+;` is the target case. Expected: 1 `delete +`,
    // producing `2 * (c - 4)`. Old behaviour: 5 edits fabricating
    // `c(,4)` as a function call.
    {"TrailingPlusBeforeSemicolonDeletes",
     "Module basicMath\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 * c-4+; // 16\n",
     /*maxEdits=*/2},
    // The exact user-reported input; the trailing `+;` line sits between
    // multiple healthy statements.
    {"TrailingPlusBeforeSemicolonInsideLargerModuleDeletes",
     "Module basicMath\n"
     "\n"
     "def a: 5;\n"
     "def b: 3;\n"
     "def c: a + b; // 8\n"
     "def d: (a ^ b); // 164\n"
     "\n"
     "def root(x, y):\n"
     "    x^(1/y);\n"
     "\n"
     "def sqrt(x):\n"
     "    root(x, 2);\n"
     "\n"
     "2 * c-4+; // 16\n"
     "b % 2; // 1\n"
     "\n"
     "Root(D, 3); // 32\n"
     "Root(64, 3); // 4\n"
     "Sqrt(81); // 9\n",
     /*maxEdits=*/2},
    {"TrailingMinusAfterPrimaryDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 + c-; // 7\n",
     /*maxEdits=*/2},
    {"TrailingStarAfterBinaryExpressionDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 + c*; // 7\n",
     /*maxEdits=*/2},
    // `2 *+ c;` — operator-doubled noise: `*` is a valid op, `+` is a
    // stray that must be deleted to reach the right operand.
    {"DoubledStarPlusOperatorDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 *+ c; // 10\n",
     /*maxEdits=*/2},
    {"DoubledMinusPlusOperatorDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 -+ c; // -3\n",
     /*maxEdits=*/2},
    {"DoubledStarMinusOperatorDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "2 *- c; // -10\n",
     /*maxEdits=*/2},
    {"DoubledSlashStarOperatorDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "10 /* c; // 2\n",
     /*maxEdits=*/2},
    {"LeadingDoubledOperatorBetweenStatementsDeletes",
     "Module m\n"
     "\n"
     "def c: 5;\n"
     "\n"
     "c;\n"
     "*+ c;\n"
     "c;\n",
     /*maxEdits=*/3},
    // `2 + ;` — operand missing between `+` and `;`. Either:
    //   * delete `+` (1 edit) — produces `2;`
    //   * insert synthetic operand (1 edit) — produces `2 + <synth>;`
    // Both are 1-edit fullMatch; the ranker picks one. Either is fine.
    {"ExpressionWithMissingRightOperandRecovers",
     "Module m\n"
     "\n"
     "2 + ;\n",
     /*maxEdits=*/2},
    // Regression: `def c: a * +++ b;` followed by a statement with a
    // trailing `// …` comment on the last line used to fragment into 6 edits
    // (Insert ';' @ 13, Insert ';' @ 14, Delete `:`, Delete `a`, Delete `*`,
    // Delete `+++`) instead of the clean 1-edit `Delete +++`.
    //
    // Root cause: Repetition's InsertRetry plan calls the InfixRule with
    // allowDelete=false, which blocked its bounded RHS-noise delete scan.
    // The InfixRule then returned unmatched and outer recovery fabricated the
    // multi-edit path. InfixRule's RHS-noise scan is a grammar-driven
    // affordance of the Pratt pattern (bounded to 4 codepoints), so it now
    // locally enables delete permission for the noise probe and replay.
    {"MultipleStrayOperatorsWithTrailingCommentRecover",
     "Module m\n"
     "def c: a * +++ b;\n"
     "def d: (a ^ b); // 164\n",
     /*maxEdits=*/2},
    // Same regression embedded in a multi-statement module: without
    // the fix, the fragmentation propagates through Module's many(Statement)
    // iteration and the output has 6+ diagnostics.
    {"MultipleStrayOperatorsInsideLargerModuleWithTrailingCommentRecover",
     "Module basicMath\n"
     "\n"
     "def a: 5;\n"
     "def b: 3;\n"
     "def c: a * +++ b; // 8\n"
     "def d: (a ^ b); // 164\n",
     /*maxEdits=*/2},
    // Two identical trailing-`+` errors separated by valid statements. The
    // first recovery commits a `delete +`; the second window must be able
    // to replay that committed delete during its re-parse and then commit a
    // second `delete +` for the second error. A regression where the new
    // window's editFloor blocks replay of earlier-window committed edits
    // would collapse the second attempt to a strict failure at the module
    // header, leaving every statement after the first error reported as
    // "Unexpected input."
    {"TwoDistantTrailingPlusErrorsBothRecover",
     "Module m\n"
     "\n"
     "1*(2-3*7+);\n"
     "1*(2);\n"
     "1*(2-3*7+);\n",
     /*maxEdits=*/3},
};

TEST(ArithmeticsOperatorRecoveryTest, OperatorRecoveryWithinBound) {
  for (const auto &c : kOperatorRecoveryCases) {
    SCOPED_TRACE(c.name);
    expect_recovers_within(std::string(c.text), c.maxEdits);
  }
}

} // namespace
} // namespace arithmetics::tests
