#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

constexpr std::string_view kDenseMissingSemicolonText =
    "Module basicMath\n"
    "\n"
    "\n"
    "def a: 5\n"
    "\n"
    "def b: 3\n"
    "def c: a + b // 8\n"
    "def d: (a ^ b) // 164\n"
    "\n"
    "def root(x, y):\n"
    "    x^(1/y)\n"
    "\n"
    "def sqrt(x):\n"
    "    root(x, 2)\n"
    "\n"
    "2 * c // 16\n"
    "b % 2 // 1\n"
    "\n"
    "\n"
    "\n"
    "// This language is case-insensitive regarding symbol names\n"
    "Root(D, 3/0); // 32\n"
    "Root(64, 3/0); // 4\n";

constexpr std::string_view kFunctionBodyMissingSemicolonText =
    "Module basicMath\n"
    "\n"
    "def a: 5;\n"
    "def b: 3;\n"
    "def c: a + b; // 8\n"
    "def d: (a ^ b); // 164\n"
    "\n"
    "def root(x, y):\n"
    "    x^(1/y)\n"
    "\n"
    "def sqrt(x):\n"
    "    root(x, 2);\n"
    "\n"
    "2 * c; // 16\n"
    "b % 2; // 1\n"
    "\n"
    "// This language is case-insensitive regarding symbol names\n"
    "Root(D, 3/0); // 32\n"
    "Root(64, 3/0); // 4\n";

TEST(ArithmeticsLanguageTest,
     DenseMissingSemicolonModuleKeepsLateCallsRecoverable) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, std::string(kDenseMissingSemicolonText),
      pegium::test::make_file_uri(
          "dense-missing-semicolon-module-keeps-late-calls-recoverable.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 8u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  ASSERT_EQ(lastCall->args.size(), 2u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     FunctionBodyMissingSemicolonKeepsLateCallsRecoverable) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, std::string(kFunctionBodyMissingSemicolonText),
      pegium::test::make_file_uri(
          "function-body-missing-semicolon-keeps-late-calls-recoverable.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 8u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  ASSERT_EQ(lastCall->args.size(), 2u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

} // namespace
} // namespace arithmetics::tests
