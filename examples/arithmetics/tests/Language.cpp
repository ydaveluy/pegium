#include <gtest/gtest.h>

#include <arithmetics/lsp/Module.hpp>
#include <arithmetics/parser/Parser.hpp>

#include "LanguageTestSupport.hpp"

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>

namespace arithmetics::tests {
namespace {

using pegium::as_services;

TEST(ArithmeticsLanguageTest,
     RecoveryAfterMissingSemicolonKeepsFollowingBrokenCallLocal) {
  parser::ArithmeticParser parser;
  const std::string prefix =
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
      "\n";
  const std::string suffix =
      "\n"
      "Root(64 3/0); // 4\n";
  auto missingSemicolonDocument = pegium::test::parse_document(
      parser, prefix + "xx" + suffix,
      pegium::test::make_file_uri("missing-semicolon-before-broken-call.calc"),
      "arithmetics");
  auto explicitSemicolonDocument = pegium::test::parse_document(
      parser, prefix + "xx;" + suffix,
      pegium::test::make_file_uri("explicit-semicolon-before-broken-call.calc"),
      "arithmetics");

  const auto &missingParsed = missingSemicolonDocument->parseResult;
  const auto &explicitParsed = explicitSemicolonDocument->parseResult;
  const auto missingDump = dump_parse_diagnostics(missingParsed.parseDiagnostics);
  const auto explicitDump =
      dump_parse_diagnostics(explicitParsed.parseDiagnostics);

  ASSERT_TRUE(missingParsed.value) << missingDump;
  ASSERT_TRUE(explicitParsed.value) << explicitDump;
  ASSERT_TRUE(missingSemicolonDocument->parseSucceeded()) << missingDump;
  ASSERT_TRUE(explicitSemicolonDocument->parseSucceeded()) << explicitDump;
  EXPECT_TRUE(missingSemicolonDocument->parseRecovered()) << missingDump;
  EXPECT_TRUE(explicitSemicolonDocument->parseRecovered()) << explicitDump;

  auto *missingModule =
      dynamic_cast<ast::Module *>(missingParsed.value.get());
  auto *explicitModule =
      dynamic_cast<ast::Module *>(explicitParsed.value.get());
  ASSERT_NE(missingModule, nullptr) << missingDump;
  ASSERT_NE(explicitModule, nullptr) << explicitDump;

  auto *missingLastEvaluation = dynamic_cast<ast::Evaluation *>(
      missingModule->statements.back().get());
  auto *explicitLastEvaluation = dynamic_cast<ast::Evaluation *>(
      explicitModule->statements.back().get());
  ASSERT_NE(missingLastEvaluation, nullptr) << missingDump;
  ASSERT_NE(explicitLastEvaluation, nullptr) << explicitDump;
  auto *missingLastCall = dynamic_cast<ast::FunctionCall *>(
      missingLastEvaluation->expression.get());
  auto *explicitLastCall = dynamic_cast<ast::FunctionCall *>(
      explicitLastEvaluation->expression.get());
  ASSERT_NE(missingLastCall, nullptr)
      << missingDump << " :: "
      << summarize_module_statement_shapes(*missingModule);
  ASSERT_NE(explicitLastCall, nullptr)
      << explicitDump << " :: "
      << summarize_module_statement_shapes(*explicitModule);
  EXPECT_EQ(missingLastCall->func.getRefText(), "root");
  EXPECT_EQ(explicitLastCall->func.getRefText(), "root");
  EXPECT_FALSE(missingLastCall->args.empty());
  EXPECT_FALSE(explicitLastCall->args.empty());
}

TEST(ArithmeticsLanguageTest, SingleBrokenCallKeepsFunctionNameLocal) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "Module basicMath\n"
      "\n"
      "Root(64 3/0); // 4\n",
      pegium::test::make_file_uri("single-broken-call.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);

  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(document->parseSucceeded()) << parseDump;
  EXPECT_TRUE(document->parseRecovered()) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_EQ(module->statements.size(), 1u) << parseDump;
  auto *evaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.front().get());
  ASSERT_NE(evaluation, nullptr) << parseDump;
  auto *call =
      dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->func.getRefText(), "root") << parseDump;
  EXPECT_FALSE(call->args.empty()) << parseDump;
}

TEST(ArithmeticsLanguageTest,
     LateStandaloneTokenDoesNotDegradeEarlierRecoveredSemicolons) {
  parser::ArithmeticParser parser;
  const std::string prefix =
      "Module basicMath\n"
      "\n"
      "def a: 5\n"
      "def b: 3\n"
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
      "\n";
  const std::string suffix =
      "\n"
      "Root(64 + 5, 3/0+5-3); // 4\n";
  const auto withStandalone = prefix + "xx" + suffix;
  const auto withCommentedStandalone = prefix + "//xx" + suffix;
  auto standaloneDocument = pegium::test::parse_document(
      parser, withStandalone,
      pegium::test::make_file_uri("late-standalone-token.calc"), "arithmetics");
  auto commentedDocument = pegium::test::parse_document(
      parser, withCommentedStandalone,
      pegium::test::make_file_uri("late-commented-token.calc"), "arithmetics");

  const auto &standaloneParsed = standaloneDocument->parseResult;
  const auto &commentedParsed = commentedDocument->parseResult;
  const auto standaloneDump =
      dump_parse_diagnostics(standaloneParsed.parseDiagnostics);
  const auto commentedDump =
      dump_parse_diagnostics(commentedParsed.parseDiagnostics);
  ASSERT_TRUE(standaloneParsed.value) << standaloneDump;
  ASSERT_TRUE(commentedParsed.value) << commentedDump;
  ASSERT_TRUE(standaloneDocument->parseSucceeded()) << standaloneDump;
  ASSERT_TRUE(commentedDocument->parseSucceeded()) << commentedDump;
  EXPECT_TRUE(standaloneDocument->parseRecovered()) << standaloneDump;
  EXPECT_TRUE(commentedDocument->parseRecovered()) << commentedDump;

  auto *standaloneModule =
      dynamic_cast<ast::Module *>(standaloneParsed.value.get());
  auto *commentedModule =
      dynamic_cast<ast::Module *>(commentedParsed.value.get());
  ASSERT_NE(standaloneModule, nullptr) << standaloneDump;
  ASSERT_NE(commentedModule, nullptr) << commentedDump;
  const auto standaloneMarker = withStandalone.find("\nxx\n");
  ASSERT_NE(standaloneMarker, std::string::npos);
  const auto standaloneOffset =
      static_cast<pegium::TextOffset>(standaloneMarker + 1u);
  const auto dump_before_offset =
      [](const std::vector<pegium::parser::ParseDiagnostic> &diagnostics,
         pegium::TextOffset offset) {
        std::vector<pegium::parser::ParseDiagnostic> prefixDiagnostics;
        for (const auto &diagnostic : diagnostics) {
          if (diagnostic.endOffset <= offset) {
            prefixDiagnostics.push_back(diagnostic);
          }
        }
        return dump_parse_diagnostics(prefixDiagnostics);
      };
  EXPECT_EQ(dump_before_offset(standaloneParsed.parseDiagnostics, standaloneOffset),
            dump_before_offset(commentedParsed.parseDiagnostics, standaloneOffset))
      << standaloneDump << " / " << commentedDump;
  EXPECT_TRUE(std::ranges::any_of(
      standaloneParsed.parseDiagnostics, [&](const auto &diag) {
        return pegium::parser::isSyntaxParseDiagnostic(diag.kind) &&
               diag.beginOffset >= standaloneOffset;
      }))
      << standaloneDump;

  auto *standaloneLastEvaluation = dynamic_cast<ast::Evaluation *>(
      standaloneModule->statements.back().get());
  auto *commentedLastEvaluation = dynamic_cast<ast::Evaluation *>(
      commentedModule->statements.back().get());
  ASSERT_NE(standaloneLastEvaluation, nullptr) << standaloneDump;
  ASSERT_NE(commentedLastEvaluation, nullptr) << commentedDump;
  EXPECT_EQ(summarize_expression(standaloneLastEvaluation->expression.get()),
            summarize_expression(commentedLastEvaluation->expression.get()))
      << standaloneDump << " / " << commentedDump;
}

TEST(ArithmeticsLanguageTest, ParsesAndEvaluatesModule) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module Demo\n"
      "def value: 1 + 2;\n"
      "value;\n",
      pegium::test::make_file_uri("language.calc"), "arithmetics");

  ASSERT_TRUE(document->parseSucceeded());
  auto *module = dynamic_cast<ast::Module *>(document->parseResult.value.get());
  ASSERT_NE(module, nullptr);

  const auto results = evaluate_module(*module);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_DOUBLE_EQ(results.front(), 3.0);
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFollowingStatementAfterMalformedStandaloneOperatorRunInLargerModule) {
  parser::ArithmeticParser parser;
  const std::string text =
      "module m\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "def c: a + b;\n"
      "def d: (a + b);\n"
      "\n"
      "def root(x, y):\n"
      "    x+(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2);\n"
      "\n"
      "2 * c;\n"
      "b % 2;\n"
      "2*********\n"
      "\n"
      "b % 2;\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri("malformed-operator-run-large.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(module->statements.size(), 9u)
      << parseDump << " :: " << summarize_module_statements(*module) << " :: "
      << summarize_module_statement_shapes(*module);
  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastBinary =
      dynamic_cast<ast::BinaryExpression *>(lastEvaluation->expression.get());
  ASSERT_NE(lastBinary, nullptr) << parseDump;
  EXPECT_EQ(lastBinary->op, "%") << parseDump;
}

TEST(ArithmeticsLanguageTest,
     MissingDefinitionColonInLargeModulePrefersLocalInsertRecovery) {
  parser::ArithmeticParser parser;
  const std::string text =
      "//\n"
      "Module basicMath\n"
      "\n"
      "\n"
      "def a 5;\n"
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
      "\n"
      "\n"
      "// This language is case-insensitive regarding symbol names\n"
      "Root(D, 3/0); // 32\n"
      "Root(64, 3/0); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text, pegium::test::make_file_uri("missing-definition-colon-large.calc"),
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
  auto *firstDefinition =
      dynamic_cast<ast::Definition *>(module->statements.front().get());
  ASSERT_NE(firstDefinition, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(firstDefinition->name, "a");
  ASSERT_NE(firstDefinition->expr, nullptr) << parseDump;
  EXPECT_EQ(summarize_expression(firstDefinition->expr.get()),
            "number:5.000000")
      << parseDump;
  EXPECT_NE(parseDump.find("Inserted:':'"), std::string::npos) << parseDump;
}

TEST(ArithmeticsLanguageTest,
     MissingSemicolonBeforeNextDefinitionWithTrailingSemicolonsPrefersLocalInsert) {
  parser::ArithmeticParser parser;
  const std::string text =
      "//\n"
      "Module basicMath\n"
      "\n"
      "\n"
      "def a: 5\n"
      "def b: 3;\n"
      ";\n"
      ";\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "missing-semicolon-before-next-definition-with-trailing-semicolons.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  const auto syntaxDiagnosticCount = std::ranges::count_if(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        return pegium::parser::isSyntaxParseDiagnostic(diagnostic.kind);
      });
  const auto insertedSemicolonCount = std::ranges::count_if(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Inserted &&
               diagnostic.beginOffset == 31u && diagnostic.endOffset == 31u &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal;
      });
  EXPECT_EQ(insertedSemicolonCount, 1) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Replaced;
      }))
      << parseDump;
  EXPECT_TRUE(std::ranges::all_of(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        if (!pegium::parser::isSyntaxParseDiagnostic(diagnostic.kind)) {
          return true;
        }
        if (diagnostic.kind == pegium::parser::ParseDiagnosticKind::Inserted) {
          return diagnostic.beginOffset >= 31u && diagnostic.endOffset >= 31u;
        }
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset >= 40u;
      }))
      << parseDump;
  EXPECT_LE(syntaxDiagnosticCount, 3) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 2u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *firstDefinition =
      dynamic_cast<ast::Definition *>(module->statements[0].get());
  auto *secondDefinition =
      dynamic_cast<ast::Definition *>(module->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  ASSERT_NE(secondDefinition, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
  EXPECT_EQ(summarize_expression(firstDefinition->expr.get()),
            "number:5.000000")
      << parseDump;
  EXPECT_EQ(summarize_expression(secondDefinition->expr.get()),
            "number:3.000000")
      << parseDump;
}

TEST(ArithmeticsLanguageTest,
     TwoMissingDefinitionColonsPreferTwoLocalInsertions) {
  parser::ArithmeticParser parser;
  const std::string text =
      "Module basicMath\n"
      "\n"
      "\n"
      "def a 5;\n"
      "def b 3;\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri("two-missing-definition-colons.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  const auto countInsertedColons = std::ranges::count_if(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset &&
               (diagnostic.beginOffset == 25u || diagnostic.beginOffset == 34u);
      });
  EXPECT_EQ(countInsertedColons, 2) << parseDump;
  EXPECT_TRUE(std::ranges::all_of(
      parsed.parseDiagnostics, [](const pegium::parser::ParseDiagnostic &diagnostic) {
        if (!pegium::parser::isSyntaxParseDiagnostic(diagnostic.kind)) {
          return true;
        }
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset &&
               (diagnostic.beginOffset == 25u || diagnostic.beginOffset == 34u);
      }))
      << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_EQ(module->statements.size(), 2u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *firstDefinition =
      dynamic_cast<ast::Definition *>(module->statements[0].get());
  auto *secondDefinition =
      dynamic_cast<ast::Definition *>(module->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  ASSERT_NE(secondDefinition, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
  EXPECT_EQ(summarize_expression(firstDefinition->expr.get()),
            "number:5.000000")
      << parseDump;
  EXPECT_EQ(summarize_expression(secondDefinition->expr.get()),
            "number:3.000000")
      << parseDump;
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFollowingDivisionStatementAfterStandaloneStarRunStatement) {
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
      "b % 2; //aaa\n"
      "\n"
      "***;\n"
      "\n"
      "b % 2/0;\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri("standalone-star-run-mid-module.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  const auto starsPos = text.find("***");
  ASSERT_NE(starsPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
           diag.beginOffset == static_cast<pegium::TextOffset>(starsPos) &&
           diag.endOffset ==
               static_cast<pegium::TextOffset>(starsPos + 4u);
  })) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(module->statements.size(), 9u)
      << parseDump << " :: " << summarize_module_statements(*module) << " :: "
      << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastBinary =
      dynamic_cast<ast::BinaryExpression *>(lastEvaluation->expression.get());
  ASSERT_NE(lastBinary, nullptr) << parseDump;
  EXPECT_EQ(lastBinary->op, "/");
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFollowingStatementAfterLongStandaloneStarRunStatement) {
  parser::ArithmeticParser parser;
  const std::string operatorRun(37u, '*');
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
      "b % 2; //aaa\n"
      "\n" +
      operatorRun +
      ";\n"
      "\n"
      "b % 2;\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri("standalone-long-star-run-mid-module.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  const auto starsPos = text.find(operatorRun);
  ASSERT_NE(starsPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
           diag.beginOffset ==
               static_cast<pegium::TextOffset>(starsPos);
  })) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
           diag.endOffset ==
               static_cast<pegium::TextOffset>(starsPos + operatorRun.size() + 1u);
  })) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  EXPECT_EQ(module->statements.size(), 9u)
      << parseDump << " :: " << summarize_module_statements(*module) << " :: "
      << summarize_module_statement_shapes(*module);
  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsRecoveredCallBeforeTrailingGarbageAndEmptyLineCommentAtEof) {
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
      "Root(64 3); // 4\n"
      "\n"
      "\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "xxxxxxxxxxxxxxxxxxxxxxx\n"
      "//\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "recovered-call-before-trailing-garbage-empty-comment.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 10u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements[9].get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsRecoveredCallBeforeTrailingGarbageWithoutEmptyLineCommentAtEof) {
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
      "Root(64 3); // 4\n"
      "\n"
      "\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "xxxxxxxxxxxxxxxxxxxxxxx\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "recovered-call-before-trailing-garbage-no-empty-comment.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 10u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements[9].get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFollowingStatementAfterLongGarbageTail) {
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
      "\n"
      "xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n"
      "xxxxxxxxxxxxxxxxxxxxxxx\n"
      "\n"
      "Root(64, 3/0); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "recovered-long-garbage-tail-followed-by-call.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 11u)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);

  auto *lastEvaluation =
      dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<ast::FunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr)
      << parseDump << " :: " << summarize_module_statement_shapes(*module);
  EXPECT_EQ(lastCall->func.getRefText(), "root");
  EXPECT_FALSE(lastCall->args.empty());
}


TEST(ArithmeticsLanguageTest,
     BrokenCallAfterComplexFirstArgumentKeepsFunctionNameWithoutGarbagePrefix) {
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
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "broken-call-after-complex-first-argument-without-garbage-prefix.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 11u)
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
  EXPECT_EQ(lastCall->args.size(), 2u);
}


TEST(ArithmeticsLanguageTest,
     ShortGarbageLineBeforeComplexBrokenCallKeepsFollowingCallRecoveryLocal) {
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
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "short-garbage-line-before-complex-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 11u)
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
     SpacedShortGarbageLineBeforeComplexBrokenCallKeepsFollowingCallRecoveryLocal) {
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
      "x    \n"
      "\n"
      "Root(64 + 5 3/0+5-3); // 4\n";
  auto document = pegium::test::parse_document(
      parser, text,
      pegium::test::make_file_uri(
          "spaced-short-garbage-line-before-complex-broken-call-keeps-following-call-local.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << parseDump;
  ASSERT_GE(module->statements.size(), 11u)
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

TEST(ArithmeticsLanguageTest, EmptyDocumentPublishesSingleGrammarDiagnosticDirectParse) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, "", pegium::test::make_file_uri("empty-direct.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  EXPECT_EQ(parsed.parseDiagnostics.size(), 1u) << parseDump;
  if (!parsed.parseDiagnostics.empty()) {
    EXPECT_EQ(parsed.parseDiagnostics.front().offset, 0u) << parseDump;
  }
}

TEST(ArithmeticsLanguageTest, CommentProviderReturnsLeadingBlockComment) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module Demo\n"
      "/** Adds numbers. */\n"
      "def value: 1;\n",
      pegium::test::make_file_uri("comments.calc"), "arithmetics");

  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services =
      arithmetics::lsp::create_language_services(*shared, "arithmetics");

  auto *module = dynamic_cast<ast::Module *>(document->parseResult.value.get());
  ASSERT_NE(module, nullptr);
  auto *definition = dynamic_cast<ast::Definition *>(module->statements.front().get());
  ASSERT_NE(definition, nullptr);

  const auto comment =
      services->documentation.commentProvider->getComment(*definition);
  EXPECT_NE(comment.find("Adds numbers."), std::string_view::npos);
}

TEST(ArithmeticsLanguageTest, DocumentationProviderRendersJSDocMarkdown) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module Demo\n"
      "/**\n"
      " * Adds numbers.\n"
      " * @param x first value\n"
      " */\n"
      "def value: 1;\n",
      pegium::test::make_file_uri("documentation.calc"), "arithmetics");

  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services =
      arithmetics::lsp::create_language_services(*shared, "arithmetics");

  auto *module = dynamic_cast<ast::Module *>(document->parseResult.value.get());
  ASSERT_NE(module, nullptr);
  auto *definition = dynamic_cast<ast::Definition *>(module->statements.front().get());
  ASSERT_NE(definition, nullptr);

  const auto documentation = services->documentation.documentationProvider
                                 ->getDocumentation(*definition);
  ASSERT_TRUE(documentation.has_value());
  EXPECT_NE(documentation->find("Adds numbers."), std::string::npos);
  EXPECT_NE(documentation->find("- `@param x first value`"), std::string::npos);
}

TEST(ArithmeticsLanguageTest, HoverReturnsNoContentWithoutDocumentation) {
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(arithmetics::lsp::register_language_services(*shared));

  const auto uri = pegium::test::make_file_uri("hover.calc");
  auto document = pegium::test::open_and_build_document(
      *shared, uri, "arithmetics",
      "Module basicMath\n"
      "\n"
      "\n"
      "def c: 2*5;\n"
      "\n"
      "2 * c + 1 - 3;\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.hoverProvider, nullptr);

  auto expect_no_hover = [&](std::uint32_t line, std::uint32_t character) {
    ::lsp::HoverParams params{};
    params.textDocument.uri = ::lsp::FileUri::parse(uri);
    params.position.line = line;
    params.position.character = character;

    auto hover =
        services->lsp.hoverProvider->getHoverContent(*document, params);
    EXPECT_FALSE(hover.has_value());
  };

  expect_no_hover(3, 1);
  expect_no_hover(5, 4);
}

TEST(ArithmeticsLanguageTest, HoverReturnsDocumentationForModuleName) {
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(arithmetics::lsp::register_language_services(*shared));

  const auto uri = pegium::test::make_file_uri("root-hover.calc");
  auto document = pegium::test::open_and_build_document(
      *shared, uri, "arithmetics",
      "/**the best module*/\n"
      "Module basicMath\n"
      "\n"
      "/** test */\n"
      "def c: 2*5;\n"
      "\n"
      "2 * c + 1 - 3;\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.hoverProvider, nullptr);

  ::lsp::HoverParams params{};
  params.textDocument.uri = ::lsp::FileUri::parse(uri);
  params.position.line = 1;
  params.position.character = 8;

  const auto hover = services->lsp.hoverProvider->getHoverContent(
      *document, params, pegium::utils::default_cancel_token);
  ASSERT_TRUE(hover.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::MarkupContent>(hover->contents));
  const auto &content = std::get<::lsp::MarkupContent>(hover->contents);
  EXPECT_NE(content.value.find("the best module"), std::string::npos);
}

TEST(ArithmeticsLanguageTest, HoverReturnsDocumentationForDefinitionName) {
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(arithmetics::lsp::register_language_services(*shared));

  const auto uri = pegium::test::make_file_uri("definition-hover.calc");
  auto document = pegium::test::open_and_build_document(
      *shared, uri, "arithmetics",
      "Module basicMath\n"
      "\n"
      "/** test */\n"
      "def c: 2*5;\n"
      "\n"
      "2 * c + 1 - 3;\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.hoverProvider, nullptr);

  ::lsp::HoverParams params{};
  params.textDocument.uri = ::lsp::FileUri::parse(uri);
  params.position.line = 3;
  params.position.character = 4;

  const auto hover = services->lsp.hoverProvider->getHoverContent(
      *document, params, pegium::utils::default_cancel_token);
  ASSERT_TRUE(hover.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::MarkupContent>(hover->contents));
  const auto &content = std::get<::lsp::MarkupContent>(hover->contents);
  EXPECT_NE(content.value.find("test"), std::string::npos);
}

TEST(ArithmeticsLanguageTest, HoverResultSerializesForDocumentedRootAndReference) {
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(arithmetics::lsp::register_language_services(*shared));

  const auto uri = pegium::test::make_file_uri("documented-hover.calc");
  auto document = pegium::test::open_and_build_document(
      *shared, uri, "arithmetics",
      "/**the best module*/\n"
      "Module basicMath\n"
      "\n"
      "/** test */\n"
      "def c: 2*5;\n"
      "\n"
      "2 * c + 1 - 3;\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.hoverProvider, nullptr);

  auto expect_serializable_hover = [&](std::uint32_t line,
                                       std::uint32_t character) {
    ::lsp::HoverParams params{};
    params.textDocument.uri = ::lsp::FileUri::parse(uri);
    params.position.line = line;
    params.position.character = character;

    auto hover =
        services->lsp.hoverProvider->getHoverContent(*document, params);
    ASSERT_TRUE(hover.has_value());

    ::lsp::TextDocument_HoverResult result{};
    result = std::move(*hover);

    const auto json = ::lsp::json::stringify(::lsp::toJson(std::move(result)));
    EXPECT_NO_THROW((void)::lsp::json::parse(json));
  };

  expect_serializable_hover(1, 8);
  expect_serializable_hover(6, 4);
}

} // namespace
} // namespace arithmetics::tests
