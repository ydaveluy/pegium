#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

TEST(ArithmeticsLanguageTest,
     EmptyCallOpenGroupAndMalformedCallsKeepFollowingStatementsDirectParse) {
  parser::ArithmeticParser parser;
  const std::string text =
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
      "2 * c; // 16\n"
      "b % 2; // 1\n"
      "\n"
      "// This language is case-insensitive regarding symbol names\n"
      "Root(D, 3); // 32\n"
      "Root(64, 3); // 4\n"
      "\n"
      "Sqrt()\n"
      "(\n"
      "root(2,);\n"
      "root(,2);\n"
      "\n"
      "sada;\n"
      "Sqrt(81/0); // 9\n";

  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "empty-call-open-group-and-malformed-calls-direct-parse.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_FALSE(module->statements.empty())
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "sqrt")
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  ASSERT_EQ(lastCall->args.size(), 1u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

} // namespace
} // namespace arithmetics::tests
