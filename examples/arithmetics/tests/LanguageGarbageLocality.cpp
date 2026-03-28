#include <gtest/gtest.h>

#include <arithmetics/lsp/Module.hpp>
#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

TEST(ArithmeticsLanguageTest,
     GarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "\n"
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
      "xxxxx\n"
      "xxxxxxxxxxxx;\n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "garbage-block-before-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 12u)
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
  EXPECT_FALSE(lastCall->args.empty())
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     ShorterGarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "\n"
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
      "xxxxx\n"
      "xxxxxxxxxxx;\n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "shorter-garbage-block-before-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 12u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}

TEST(ArithmeticsLanguageTest,
     SplitGarbageBlockBeforeBrokenCallKeepsFollowingCallRecoveryLocal) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "\n"
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
      "xx\n"
      "xxxxxxxxxxxx;\n"
      "xx\n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "split-garbage-block-before-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 12u)
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
  EXPECT_FALSE(lastCall->args.empty())
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     SpacedGarbageRunBeforeComplexBrokenCallKeepsFollowingCallRecoveryLocal) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "\n"
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
      "xx\n"
      "xxxxxxxxxxxx;\n"
      "x    x x x x x x x\n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "spaced-garbage-run-before-complex-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 13u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}

TEST(ArithmeticsLanguageTest,
     ShorterSpacedGarbageRunBeforeComplexBrokenCallKeepsFollowingCallRecoveryLocal) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "\n"
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
      "xx\n"
      "xxxxxxxxxxxx;\n"
      "x    x x x x x x\n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "shorter-spaced-garbage-run-before-complex-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 13u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}

} // namespace
} // namespace arithmetics::tests
