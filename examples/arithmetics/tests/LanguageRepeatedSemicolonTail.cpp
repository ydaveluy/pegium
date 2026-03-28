#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

constexpr std::string_view kRepeatedMissingSemicolonTailText =
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
    "Sqrt(81); // 9\n"
    "Root(D, 3); // 32\n"
    "Root(64, 3)\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n";

constexpr std::string_view kLongRepeatedMissingSemicolonTailText =
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
    "Sqrt(81); // 9\n"
    "Root(D, 3); // 32\n"
    "Root(64, 3)\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81); // 9\n"
    "Sqrt(81); // 9\n"
    "Sqrt(81); // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81) // 9\n"
    "Sqrt(81); // 9\n";

TEST(ArithmeticsLanguageTest,
     RepeatedMissingSemicolonTailRecoversCompletelyWithInsertOnlyEdits) {
  parser::ArithmeticParser defaultParser;

  auto defaultDocument = pegium::test::parse_document(
      defaultParser, std::string(kRepeatedMissingSemicolonTailText),
      pegium::test::make_file_uri("repeated-missing-semicolon-tail-default.calc"),
      "arithmetics");

  const auto &defaultParsed = defaultDocument->parseResult;
  const auto defaultDump = dump_parse_diagnostics(defaultParsed.parseDiagnostics);

  ASSERT_TRUE(defaultParsed.value) << defaultDump;
  EXPECT_TRUE(defaultParsed.fullMatch) << defaultDump;
  EXPECT_TRUE(defaultParsed.recoveryReport.hasRecovered) << defaultDump;
  EXPECT_FALSE(std::ranges::any_of(
      defaultParsed.parseDiagnostics, [](const auto &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Incomplete;
      }))
      << defaultDump;

  auto *defaultModule = dynamic_cast<ast::Module *>(defaultParsed.value.get());
  ASSERT_NE(defaultModule, nullptr) << defaultDump;
  EXPECT_GE(defaultModule->statements.size(), 25u)
      << summarize_module_statement_shapes(*defaultModule);
  EXPECT_FALSE(std::ranges::any_of(
      defaultParsed.parseDiagnostics, [](const auto &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Deleted;
      }))
      << defaultDump;
}

TEST(ArithmeticsLanguageTest,
     LongRepeatedMissingSemicolonTailRecoversCompletelyWithDefaultPlanner) {
  parser::ArithmeticParser parser;

  auto document = pegium::test::parse_document(
      parser, std::string(kLongRepeatedMissingSemicolonTailText),
      pegium::test::make_file_uri("long-repeated-missing-semicolon-tail.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto dump = dump_parse_diagnostics(parsed.parseDiagnostics);

  ASSERT_TRUE(parsed.value) << dump;
  EXPECT_TRUE(parsed.fullMatch) << dump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << dump;
  EXPECT_FALSE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Incomplete;
      }))
      << dump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << dump;
  EXPECT_GE(module->statements.size(), 30u)
      << summarize_module_statement_shapes(*module);
  EXPECT_FALSE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Deleted;
      }))
      << dump;
}

} // namespace
} // namespace arithmetics::tests
