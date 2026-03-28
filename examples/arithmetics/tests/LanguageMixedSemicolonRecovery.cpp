#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

TEST(ArithmeticsLanguageTest,
     MixedMissingSemicolonsKeepFollowingDefinitionsAndFunctionRecoverable) {
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
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(summarize_module_statements(*module), "def:a | def:b | def:c | def:d | def:root")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
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
  EXPECT_EQ(parsed.recoveryReport.recoveryCount, 2u) << parseDump;
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(summarize_module_statements(*module),
            "def:a | def:b | def:c | def:d | def:root | def:sqrt")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     MissingFunctionBodySemicolonAtEofKeepsFinalFunctionRecoverable) {
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
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(summarize_module_statements(*module), "def:root | def:sqrt")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     MissingSemicolonsAcrossDefinitionAndEvaluationsKeepTrailingCallsRecoverable) {
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
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(summarize_module_statements(*module),
            "def:a | def:b | def:c | def:d | def:root | def:sqrt | eval | eval | eval | eval | eval")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

} // namespace
} // namespace arithmetics::tests
