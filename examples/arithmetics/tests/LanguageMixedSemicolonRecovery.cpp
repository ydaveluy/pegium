#include <gtest/gtest.h>

#include <arithmetics/core/ArithmeticParser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

#include <functional>
#include <ranges>
#include <string>

namespace arithmetics::tests {
namespace {

// Folds the seven mixed-semicolon recovery happy-path tests that share the
// identical 6-assertion sequence (value / fullMatch / hasRecovered /
// recoveryAttemptRuns < 256 / module cast / summarize_module_statements),
// varying only the input source text and the expected summary string.
//
// NOTE: MissingDefinitionAndFunctionSemicolonsKeepTrailingFunctionRecoverable
// is intentionally NOT folded here: it adds an extra inserted-semicolon-count
// assertion, so its assertion shape differs and it remains its own test below.
TEST(ArithmeticsLanguageTest,
     MixedSemicolonRecoveryHappyPath) {
  struct Outcome {
    bool value;
    bool fullMatch;
    bool hasRecovered;
    unsigned recoveryAttemptRuns;
    ast::Module *module;
    std::string parseDump;
    std::string summary;
    std::string shapes;
  };
  struct Case {
    const char *name;
    std::function<Outcome()> run;
    const char *expectedSummary;
  };

  static const Case kCases[] = {
      {"MixedMissingSemicolonsKeepFollowingDefinitionsAndFunctionRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "//\n"
             "module basicMath\n"
             "\n"
             "\n"
             "def a: 5\n"
             "\n"
             "def b: 3;\n"
             "def c: a + b // 8\n"
             "def d: (a ^ b); // 164\n"
             "\n"
             "def root(x, y):\n"
             "    x^(1/y);\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "mixed-missing-semicolons-keep-following-definitions-and-function-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:c | def:d | def:root"},
      {"MissingFunctionBodySemicolonAtEofKeepsFinalFunctionRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def root(x, y):\n"
             "    x^(1/y);\n"
             "\n"
             "def sqrt(x):\n"
             "    root(x, 2)\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "missing-function-body-semicolon-at-eof-keeps-final-function-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:root | def:sqrt"},
      {"MissingSemicolonsAcrossDefinitionAndEvaluationsKeepTrailingCallsRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def a: 5;\n"
             "def b: 3\n"
             "def c: a + b; // 8\n"
             "def d: (a ^ b); // 164\n"
             "\n"
             "def root(x, y):\n"
             "    x^(1/y);\n"
             "\n"
             "def sqrt(x):\n"
             "    root(x, 2)\n"
             "\n"
             "2 * c // 16\n"
             "b % 2; // 1\n"
             "\n"
             "// This language is case-insensitive regarding symbol names\n"
             "Root(D, 3) // 32\n"
             "Root(64, 3); // 4\n"
             "Sqrt(81) // 9\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "missing-semicolons-across-definition-and-evaluations-keep-trailing-calls-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:c | def:d | def:root | def:sqrt | eval | eval | eval | eval | eval"},
      {"MissingDefinitionSemicolonsKeepTrailingFunctionsAndCallsRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def a: 5\n"
             "def b: 3\n"
             "def c: a + b // 8\n"
             "def d: (a ^ b); // 164\n"
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
             "// This language is case-insensitive regarding symbol names\n"
             "Root(D, 3); // 32\n"
             "Root(64, 3); // 4\n"
             "Sqrt(81); // 9\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "missing-definition-semicolons-keep-trailing-functions-and-calls-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:c | def:d | def:root | def:sqrt | eval | eval | eval | eval | eval"},
      {"MixedDefinitionSemicolonsKeepTrailingFunctionsAndCallsRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def a: 5;\n"
             "def b: 3;\n"
             "def b1: 3\n"
             "def b2: 3\n"
             "def c: a + b // 8\n"
             "def d: (a ^ b); // 164\n"
             "\n"
             "def root(x, y)\n"
             "    x^(1/y)\n"
             "\n"
             "def sqrt(x):\n"
             "    root(x, 2);\n"
             "\n"
             "2 * c; // 16\n"
             "b % 2; // 1\n"
             "\n"
             "// This language is case-insensitive regarding symbol names\n"
             "Root(D, 3); // 32\n"
             "Root(64, 3); // 4\n"
             "Sqrt(81); // 9\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "mixed-definition-semicolons-keep-trailing-functions-and-calls-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:b1 | def:b2 | def:c | def:d | def:root | def:sqrt | eval | eval | eval | eval | eval"},
      {"MixedDefinitionColonSemicolonsKeepTrailingFunctionsAndCallsRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def a 5\n"
             "def b 3\n"
             "def b1 3\n"
             "def b2: 3\n"
             "def c: a + b // 8\n"
             "def d: (a ^ b); // 164\n"
             "\n"
             "def root(x, y)\n"
             "    x^(1/y)\n"
             "\n"
             "def sqrt(x):\n"
             "    root(x, 2);\n"
             "\n"
             "2 * c; // 16\n"
             "b % 2; // 1\n"
             "\n"
             "// This language is case-insensitive regarding symbol names\n"
             "Root(D, 3); // 32\n"
             "Root(64, 3); // 4\n"
             "Sqrt(81); // 9\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "mixed-definition-colon-semicolons-keep-trailing-functions-and-calls-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:b1 | def:b2 | def:c | def:d | def:root | def:sqrt | eval | eval | eval | eval | eval"},
      {"MixedMissingDefinitionColonKeepsTrailingFunctionsAndEvaluationsRecoverable",
       [] {
         parser::ArithmeticParser parser;
         const std::string text =
             "Module basicMath\n"
             "\n"
             "def a: 5\n"
             "def b: 3\n"
             "def b1 3\n"
             "def b2: 3\n"
             "def c: a + b // 8\n"
             "def d: (a ^ b); // 164\n"
             "\n"
             "def root(x, y)\n"
             "    x^(1/y)\n"
             "\n"
             "def sqrt(x):\n"
             "    root(x, 2);\n"
             "\n"
             "2 * c; // 16\n"
             "b % 2; // 1\n";

         auto document = pegium::test::parse_document(
             parser, text,
             pegium::test::make_file_uri(
                 "mixed-missing-definition-colon-keeps-trailing-functions-and-evaluations-recoverable.calc"),
             "arithmetics");

         const auto &parsed = document->parseResult;
         auto *module = dynamic_cast<ast::Module *>(parsed.value);
         return Outcome{
             static_cast<bool>(parsed.value),
             parsed.fullMatch,
             parsed.recoveryReport.hasRecovered,
             parsed.recoveryReport.recoveryAttemptRuns,
             module,
             dump_parse_diagnostics(parsed.parseDiagnostics),
             module != nullptr ? summarize_module_statements(*module)
                               : std::string(),
             module != nullptr ? summarize_module_statement_shapes(*module)
                               : std::string()};
       },
       "def:a | def:b | def:b1 | def:b2 | def:c | def:d | def:root | def:sqrt | eval | eval"},
  };

  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    const Outcome o = c.run();
    const std::string &parseDump = o.parseDump;
    ASSERT_TRUE(o.value) << parseDump;
    EXPECT_TRUE(o.fullMatch) << parseDump;
    EXPECT_TRUE(o.hasRecovered) << parseDump;
    EXPECT_LT(o.recoveryAttemptRuns, 256u)
        << "recoveryAttemptRuns regressed\n" << parseDump;

    ASSERT_NE(o.module, nullptr) << parseDump;
    EXPECT_EQ(o.summary, c.expectedSummary)
        << parseDump << " :: " << o.shapes;
  }
}

TEST(ArithmeticsLanguageTest,
     MissingDefinitionAndFunctionSemicolonsKeepTrailingFunctionRecoverable) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3\n"
      "def c: a + b; // 8\n"
      "def d: (a ^ b); // 164\n"
      "\n"
      "def root(x, y):\n"
      "    x^(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2)\n";

  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "missing-definition-and-function-semicolons-keep-trailing-function-recoverable.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  const auto insertedSemicolonCount = std::ranges::count_if(
      parsed.parseDiagnostics,
      [](const pegium::parser::ParseDiagnostic &diagnostic) {
        return diagnostic.kind ==
                   pegium::parser::ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal;
      });
  EXPECT_EQ(insertedSemicolonCount, 2) << parseDump;
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;
  EXPECT_LT(parsed.recoveryReport.recoveryAttemptRuns, 256u)
      << "recoveryAttemptRuns regressed\n" << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value);
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(summarize_module_statements(*module),
            "def:a | def:b | def:c | def:d | def:root | def:sqrt")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

} // namespace
} // namespace arithmetics::tests
