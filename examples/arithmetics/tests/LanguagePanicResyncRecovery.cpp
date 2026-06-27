#include <gtest/gtest.h>

#include <arithmetics/core/ast.hpp>
#include <arithmetics/core/CoreModule.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/core/parser/ParseDiagnostics.hpp>

#include <string>
#include <string_view>

namespace arithmetics::tests {
namespace {

/// Panic-resync regression corpus. When a single statement is locally
/// unrecoverable (e.g. contains a doubled operator mid-expression like
/// `5 * ++ 5`), the outer `many(Statement)` must resync at the next
/// statement start instead of declaring the rest of the module
/// `Incomplete`. The resync is driven by Repetition's last-resort
/// `ResyncSkip` candidate: scan forward up to
/// `ParseOptions::maxResyncSkipCodepoints` codepoints, stop at the first
/// position where the iteration strictly starts, emit one fused
/// `Delete` over the skipped range.
///
/// These tests pin the distant-recovery property:
/// the broken statement may be bracketed by any number of healthy
/// statements, every statement past the broken one must still land in
/// the parsed AST.

void expect_fullmatch_with_statement_count(const std::string &text,
                                           std::size_t expectedMinStatements) {
  auto parser = createArithmeticsParser();
  auto parsed = parser->parse(text);
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_EQ(parsed.parsedLength, text.size()) << parseDump;
  // Panic resync plus zero or more healthy sites must stay well inside
  // the global attempt budget. 512 is a cost-of-regression signal: the
  // observed values sit below 100 on the current engine.
  EXPECT_LT(parsed.recoveryReport.recoveryAttemptRuns, 512u)
      << "recoveryAttemptRuns regressed\n" << parseDump;
  const auto *module =
      dynamic_cast<const ast::Module *>(parsed.value);
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_GE(module->statements.size(), expectedMinStatements)
      << "panic-mode resync lost statements\n" << parseDump;
}

TEST(ArithmeticsPanicResyncRecoveryTest, ResyncStatementCountCorpus) {
  struct Case {
    const char *name;
    std::string_view text;
    std::size_t minStatements;
  };
  static constexpr Case kCases[] = {
      // `def a: 5 * ++ 5;` is locally unrecoverable — `* ++` is a two-edit
      // trap the normal ranker rejects. Panic resync skips past the broken
      // body, the module keeps every def/eval after.
      {"DoubledPlusMidStatementKeepsSurroundingStatements",
       "Module basicMath\n"
       "\n"
       "def a: 5 * ++ 5;\n"
       "def b: 3;\n"
       "def c: a + b;\n"
       "def d: (a ^ b);\n"
       "\n"
       "def root(x, y):\n"
       "    x^(1/y);\n"
       "\n"
       "def sqrt(x):\n"
       "    root(x, 2);\n"
       "\n"
       "2 * c; // 16\n"
       "b % 2; // 1\n"
       "\n"
       "Root(D, 3); // 32\n"
       "Root(64, 3); // 4\n"
       "Sqrt(81); // 9\n",
       /*expectedMinStatements=*/11},
      // Minimal reproducer: the broken def is the first statement; every
      // statement after it must still parse.
      {"BrokenStatementBeforeHealthyTailReachesFullMatch",
       "Module m\n"
       "def a: 5 * ++ 5;\n"
       "def b: 3;\n"
       "def c: 8;\n",
       /*expectedMinStatements=*/3},
      {"HealthyBetweenBrokenSurvivesResync",
       "Module m\n"
       "def a: 5;\n"
       "def b: 5 * ++ 5;\n"
       "def c: 8;\n",
       /*expectedMinStatements=*/3},
      // Multi-site recovery: two unrelated broken statements in the same
      // input. The InfixRule strict-probe + panic resync makes
      // both sites resync independently. Each broken def becomes a single
      // fused delete; `def a`, `def b`, `def c`, `2 ** 3` evaluate all
      // land in the AST.
      {"TwoSuccessiveBrokenStatementsBothResync",
       "Module m\n"
       "def a: 5 * ++ 5;\n"
       "def b: 3;\n"
       "2 ** 3;\n"
       "def c: 8;\n",
       /*expectedMinStatements=*/4},
      // Both defs contain the doubled-operator pattern; the healthy def
      // in the middle plus the trailing def c all still land in the AST.
      {"TwoBrokenStatementsBracketingHealthyOneResync",
       "Module m\n"
       "def a: 5 * ++ 5;\n"
       "def b: 5 * ++ 5;\n"
       "def c: 8;\n",
       /*expectedMinStatements=*/3},
  };
  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    expect_fullmatch_with_statement_count(std::string(c.text), c.minStatements);
  }
}

TEST(ArithmeticsPanicResyncRecoveryTest,
     ResyncSkipPreservesDownstreamReferences) {
  // Reference `c` is defined after a broken statement. Panic-skip
  // ensures `def c` lands in the AST so later uses are resolvable.
  auto parser = createArithmeticsParser();
  const std::string text =
      "Module m\n"
      "def a: 5 * ++ 5;\n"
      "def c: 8;\n"
      "2 * c;\n";
  auto parsed = parser->parse(text);
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.fullMatch) << parseDump;
  const auto *module =
      dynamic_cast<const ast::Module *>(parsed.value);
  ASSERT_NE(module, nullptr) << parseDump;
  bool foundDefC = false;
  for (const auto &s : module->statements) {
    if (const auto *def = dynamic_cast<const ast::Definition *>(s)) {
      if (def->name == "c") {
        foundDefC = true;
        break;
      }
    }
  }
  EXPECT_TRUE(foundDefC) << parseDump;
}

// -----------------------------------------------------------------------------
// A typo on the entry rule's leading keyword combined with downstream
// broken statements must recover both — not give up at offset 0.
//
// When the strict parse fails at offset 0 because the entry-rule keyword
// is a fuzzy variant (e.g., `Moule` vs `module`) AND subsequent statements
// are also broken, the recovery search must still commit the offset-0
// fuzzy Replace. `satisfies_non_credible_fallback_contract` admits an
// offset-0 Replace whose post-replace region produced strict progress:
// the post-replace region IS the de-facto stable continuation, and
// downstream recovery windows can handle the broken statements as usual.
// -----------------------------------------------------------------------------

TEST(ArithmeticsPanicResyncRecoveryTest,
     RootKeywordTypoCombinedWithDownstreamBrokenStatementsRecovers) {
  auto parser = createArithmeticsParser();
  // `Moule` is a 1-edit fuzzy variant of `module`. The two `def`
  // statements are both missing their `:` and `;`. The recovery must
  // commit both the keyword AND the broken statements end-to-end.
  const std::string text =
      "Moule basicMath\n"
      "def a 5\n"
      "def b 3\n";
  auto parsed = parser->parse(text);
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.fullMatch) << parseDump;
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_EQ(parsed.parsedLength, text.size()) << parseDump;
  // 1 Replace (Moule→module) + 4 Inserts (2× : and 2× ;) = 5 edits.
  EXPECT_EQ(parsed.recoveryReport.recoveryEdits, 5u) << parseDump;

  const auto *module =
      dynamic_cast<const ast::Module *>(parsed.value);
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(module->name, "basicmath");
  ASSERT_EQ(module->statements.size(), 2u) << parseDump;
  // Both definitions are recovered with their names intact.
  const auto *defA =
      dynamic_cast<const ast::Definition *>(module->statements[0]);
  const auto *defB =
      dynamic_cast<const ast::Definition *>(module->statements[1]);
  ASSERT_NE(defA, nullptr) << parseDump;
  ASSERT_NE(defB, nullptr) << parseDump;
  EXPECT_EQ(defA->name, "a");
  EXPECT_EQ(defB->name, "b");
}

} // namespace
} // namespace arithmetics::tests
