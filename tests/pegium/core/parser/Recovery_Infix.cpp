#include "RecoveryTestSupport.hpp"

using namespace pegium::parser;
using namespace pegium::test::recovery;

TEST(RecoveryTest, OptionalInfixExpressionKeepsRecoveredPrefixBeforeDelimiter) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation", option(assign<&RecoveryExpressionEvaluation::expression>(
                        expressionRule)) +
                        ";"_kw};

  const auto result = parseRule(evaluation, "81/;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                               ParseDiagnosticKind::Deleted &&
                                           diagnostic.offset == 2u;
                                  }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value);
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  ASSERT_NE(parsedEvaluation->expression, nullptr) << parseDump;
  auto *parsedNumber = dynamic_cast<RecoveryNumberExpression *>(
      parsedEvaluation->expression);
  ASSERT_NE(parsedNumber, nullptr) << parseDump;
  EXPECT_EQ(parsedNumber->value, 81);
}

TEST(RecoveryTest, InfixRuleProbeCanSeeStartedPrimaryWithMissingRhs) {
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};

  auto builderHarness = pegium::test::makeCstBuilderHarness("81/");
  auto &builder = builderHarness.builder;
  const auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());
  RecoveryContext ctx{builder, skipper, recorder};

  ctx.skip();

  EXPECT_TRUE(probe_locally_recoverable(expression, ctx));
  EXPECT_TRUE(probe_locally_recoverable(expressionRule, ctx));
}

TEST(RecoveryTest,
     MissingRequiredExpressionCanRecoverBeforeLongPunctuationRunAndDelimiter) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const std::string operatorRun(37u, '*');
  const auto result = parseRule(evaluation, operatorRun + ";", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [&](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset == 0u &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(operatorRun.size());
      }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value);
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
}

TEST(RecoveryTest, UnexpectedPrimaryBeforeOperatorStillReportsSyntaxRecovery) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", append<&RecoveryModule::statements>(evaluation) +
                    many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "1 2*3;", skipper);

  ASSERT_FALSE(result.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [](const ParseDiagnostic &diagnostic) { return diagnostic.isSyntax(); }));
}

TEST(RecoveryTest, UnexpectedTokenAfterOperatorUsesGenericDeleteInPrimary) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expressionRule) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const std::string text = "module calc\n"
                           "def c: 8;\n"
                           "2 * +c;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  const auto plusPos = text.find('+');
  ASSERT_NE(plusPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [plusPos](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(plusPos) &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(plusPos + 1u);
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1]);
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression);
  ASSERT_NE(binary, nullptr) << parseDump;
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left);
  auto *right =
      dynamic_cast<RecoveryReferenceExpression *>(binary->right);
  ASSERT_NE(left, nullptr) << parseDump;
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2) << parseDump;
  EXPECT_EQ(binary->op, "*") << parseDump;
  EXPECT_EQ(right->name, "c") << parseDump;
}

TEST(RecoveryTest,
     LongDeleteRunAfterOperatorKeepsNextStatementOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expressionRule) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const std::string text = "module calc\n"
                           "def a: 5;\n"
                           "\n"
                           "2*7+++++++++;\n"
                           "\n"
                           "def b: 5;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  const auto plusPos = text.find("+++++++++");
  ASSERT_NE(plusPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [plusPos](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(plusPos) &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(
                       plusPos + std::string_view{"+++++++++"}.size());
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1]);
  ASSERT_NE(evaluationNode, nullptr);
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression);
  ASSERT_NE(binary, nullptr);
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left);
  auto *right = dynamic_cast<RecoveryNumberExpression *>(binary->right);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2);
  EXPECT_EQ(binary->op, "*");
  EXPECT_EQ(right->value, 7);

  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithExpr *>(
      parsedModule->statements[2]);
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(
    RecoveryTest,
    LongDeleteRunBeyondDefaultBudgetKeepsNextStatementOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryDefinitionWithExpr> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithExpr::name>(id) + ":"_kw +
          assign<&RecoveryDefinitionWithExpr::expr>(expressionRule) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition |
                                                             evaluationRule))};

  const std::string text = "module calc\n"
                           "def a: 5;\n"
                           "\n"
                           "2*7+++++++++++++++++++++++++++++++++++;\n"
                           "\n"
                           "def b: 5;\n";
  const auto result = parseRule(module, text, skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  const auto plusPos = text.find("+++++++++++++++++++++++++++++++++++");
  ASSERT_NE(plusPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [plusPos](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(plusPos) &&
               diagnostic.endOffset ==
                   static_cast<pegium::TextOffset>(
                       plusPos +
                       std::string_view{"+++++++++++++++++++++++++++++++++++"}
                           .size());
      }));

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_EQ(parsedModule->statements.size(), 3u);

  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1]);
  ASSERT_NE(evaluationNode, nullptr);
  auto *binary = dynamic_cast<RecoveryBinaryExpression *>(
      evaluationNode->expression);
  ASSERT_NE(binary, nullptr);
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left);
  auto *right = dynamic_cast<RecoveryNumberExpression *>(binary->right);
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(left->value, 2);
  EXPECT_EQ(binary->op, "*");
  EXPECT_EQ(right->value, 7);

  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithExpr *>(
      parsedModule->statements[2]);
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(
    RecoveryTest,
    LongOperatorRunWithoutDelimiterKeepsMultipleFollowingStatementsOutsideRecoveredExpression) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  const std::string operatorRun(95u, '*');
  const std::string text = "module calc\n"
                           "2;\n"
                           "2" +
                           operatorRun +
                           "\n"
                           "b%2;\n"
                           "b%2;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  const auto starsPos = text.find(operatorRun);
  ASSERT_NE(starsPos, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [&](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(starsPos) &&
               diagnostic.endOffset == static_cast<pegium::TextOffset>(
                                           starsPos + operatorRun.size());
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 4u) << parseDump;
}

TEST(RecoveryTest,
     InfixGluedOperatorNoiseAbuttingPrimaryKeepsTheRhs) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  // `2 +++++++++c` — one real `+` operator then EIGHT stray `+` glued directly
  // to the operand `c` (no skipper trivia between the noise and `c`). The
  // faithful repair keeps `2 + c` by deleting the eight stray operators. The
  // pre-fix cap (maxConsecutiveCodepointDeletes - 1 = 7) misses the operand by
  // exactly one and instead deletes the whole run + fabricates the RHS. The
  // operand abuts the noise, so absorbing it cannot merge a following
  // statement (contrast LongOperatorRun..., where a newline guards the operand).
  const std::string text = "module m\n2 +++++++++c;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[0]);
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *binary =
      dynamic_cast<RecoveryBinaryExpression *>(evaluationNode->expression);
  ASSERT_NE(binary, nullptr) << parseDump;
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left);
  auto *right =
      dynamic_cast<RecoveryReferenceExpression *>(binary->right);
  ASSERT_NE(left, nullptr) << parseDump;
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2) << parseDump;
  EXPECT_EQ(binary->op, "+") << parseDump;
  EXPECT_EQ(right->name, "c") << parseDump;
}

TEST(RecoveryTest,
     InfixNonOperatorNoiseBeforePrimaryKeepsTheRhs) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  // `2 * ... c` — non-operator garbage (`.` is no operator and no primary) sits
  // between the operator `*` and its operand `c`, separated by trivia. The same
  // garbage is deleted everywhere else (before a keyword, a statement, a def
  // body, or the infix LHS), so the infix RHS must delete it too and keep
  // `2 * c` rather than abandon the operator.
  const std::string text = "module m\n2 * ... c;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[0]);
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *binary =
      dynamic_cast<RecoveryBinaryExpression *>(evaluationNode->expression);
  ASSERT_NE(binary, nullptr) << parseDump;
  auto *left = dynamic_cast<RecoveryNumberExpression *>(binary->left);
  auto *right =
      dynamic_cast<RecoveryReferenceExpression *>(binary->right);
  ASSERT_NE(left, nullptr) << parseDump;
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(left->value, 2) << parseDump;
  EXPECT_EQ(binary->op, "*") << parseDump;
  EXPECT_EQ(right->name, "c") << parseDump;
}

TEST(RecoveryTest,
     InfixNonOperatorPunctNoiseBeforePrimaryKeepsTheRhs) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  // Same gap as the `...` case but with different garbage codepoints — the
  // cleanup must be grammar-agnostic, not tuned to any specific noise byte.
  const std::string text = "module m\n2 * ?? c;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[0]);
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *binary =
      dynamic_cast<RecoveryBinaryExpression *>(evaluationNode->expression);
  ASSERT_NE(binary, nullptr) << parseDump;
  auto *right =
      dynamic_cast<RecoveryReferenceExpression *>(binary->right);
  ASSERT_NE(right, nullptr) << parseDump;
  EXPECT_EQ(binary->op, "*") << parseDump;
  EXPECT_EQ(right->name, "c") << parseDump;
}

TEST(RecoveryTest,
     InfixNoiseCleanupDoesNotSwallowStatementTerminator) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw), LeftAssociation("%"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluationRule{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluationRule))};

  // SAFETY: `2 * ... ; b` — the `;` terminates the first (broken) statement.
  // The RHS noise cleanup must NOT delete the `;` and fold `b` into the first
  // expression. `b` must survive as its own statement.
  const std::string text = "module m\n2 * ... ; b;\n";
  const auto result = parseRule(module, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;
  auto *bStatement = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[1]);
  ASSERT_NE(bStatement, nullptr) << parseDump;
  auto *bRef =
      dynamic_cast<RecoveryReferenceExpression *>(bStatement->expression);
  ASSERT_NE(bRef, nullptr) << parseDump;
  EXPECT_EQ(bRef->name, "b") << parseDump;
}

TEST(RecoveryTest,
     IncompleteDiagnosticAfterInfixOperatorUsesFailureTokenAnchor) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                     assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "module name\n2   *", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_EQ(result.parseDiagnostics.size(), 1u) << parseDump;
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::Incomplete);
  EXPECT_EQ(result.parseDiagnostics.front().offset, 17u) << parseDump;
}

TEST(RecoveryTest,
     RepetitionRecoveryDoesNotStopOnRewoundFailureInsideActiveWindow) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryNumberExpression>() +
                         assign<&RecoveryNumberExpression::value>(number) |
                     create<RecoveryReferenceExpression>() +
                         assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      expression{"Expression", primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expressionRule{"Expression", expression};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expressionRule) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module calc\n"
                                "1+2;v\n"
                                "3+4;\n"
                                "5+6;\n",
                                skipper);

  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.fullMatch);
  EXPECT_FALSE(result.parseDiagnostics.empty());

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value);
  ASSERT_NE(parsedModule, nullptr);
  ASSERT_GE(parsedModule->statements.size(), 3u);
}

TEST(RecoveryTest,
     StarRepetitionDoesNotDisappearWhenFirstIterationAlreadyStarted) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  DataTypeRule<std::string> qualifiedName{"QualifiedName", some(id, "."_kw)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(qualifiedName)};
  ParserRule<RecoveryFeatureListNode> featureBlock{
      "FeatureBlock",
      "{"_kw + many(append<&RecoveryFeatureListNode::features>(feature)) +
          "}"_kw};

  const auto result = parseRule(featureBlock,
                                "{\n"
                                "  many comments Comment\n"
                                "  title: String\n"
                                "}\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedBlock =
      dynamic_cast<RecoveryFeatureListNode *>(result.value);
  ASSERT_NE(parsedBlock, nullptr);
  ASSERT_EQ(parsedBlock->features.size(), 2u) << parseDump;
  ASSERT_NE(parsedBlock->features[0], nullptr);
  ASSERT_NE(parsedBlock->features[1], nullptr);
  EXPECT_TRUE(parsedBlock->features[0]->many);
  EXPECT_EQ(parsedBlock->features[0]->name, "comments");
  EXPECT_EQ(parsedBlock->features[0]->type, "Comment") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->name, "title") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->type, "String") << parseDump;
}

TEST(
    RecoveryTest,
    BoundedNullableRepetitionDoesNotDisappearWhenFirstIterationAlreadyStarted) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  DataTypeRule<std::string> qualifiedName{"QualifiedName", some(id, "."_kw)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(qualifiedName)};
  ParserRule<RecoveryFeatureListNode> featureBlock{
      "FeatureBlock",
      "{"_kw +
          repeat<0, 3>(append<&RecoveryFeatureListNode::features>(feature)) +
          "}"_kw};

  const auto result = parseRule(featureBlock,
                                "{\n"
                                "  many comments Comment\n"
                                "  title: String\n"
                                "}\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedBlock =
      dynamic_cast<RecoveryFeatureListNode *>(result.value);
  ASSERT_NE(parsedBlock, nullptr);
  ASSERT_EQ(parsedBlock->features.size(), 2u) << parseDump;
  ASSERT_NE(parsedBlock->features[0], nullptr);
  ASSERT_NE(parsedBlock->features[1], nullptr);
  EXPECT_TRUE(parsedBlock->features[0]->many);
  EXPECT_EQ(parsedBlock->features[0]->name, "comments");
  EXPECT_EQ(parsedBlock->features[0]->type, "Comment") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->name, "title") << parseDump;
  EXPECT_EQ(parsedBlock->features[1]->type, "String") << parseDump;
}

TEST(
    RecoveryTest,
    NullableRepetitionCanDeleteGarbageBeforeRecoverableIterationAndRequiredSuffix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionNode> transition{
      "Transition", assign<&RecoveryTransitionNode::event>(id) + "=>"_kw +
                        assign<&RecoveryTransitionNode::target>(id)};
  ParserRule<RecoveryStateNode> state{
      "State", "state"_kw + assign<&RecoveryStateNode::name>(id) +
                   many(append<&RecoveryStateNode::transitions>(transition)) +
                   "end"_kw};

  const std::string text = "state Idle\n"
                           "<<<<<<<<<<<<<<<<<<<<<<<<\n"
                           "Start => Idle\n"
                           "end\n";
  const auto result = parseRule(state, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  const auto garbageBegin = text.find("<<<<<<<<<<<<<<<<<<<<<<<<");
  ASSERT_NE(garbageBegin, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [garbageBegin](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(garbageBegin);
      }))
      << parseDump;

  auto *parsedState = dynamic_cast<RecoveryStateNode *>(result.value);
  ASSERT_NE(parsedState, nullptr) << parseDump;
  EXPECT_EQ(parsedState->name, "Idle");
  ASSERT_EQ(parsedState->transitions.size(), 1u) << parseDump;
  ASSERT_NE(parsedState->transitions.front(), nullptr) << parseDump;
  EXPECT_EQ(parsedState->transitions.front()->event, "Start");
  EXPECT_EQ(parsedState->transitions.front()->target, "Idle");
}

TEST(RecoveryTest,
     RepetitionDeleteRetryDoesNotCrossStructuredSuffixAfterRecoveredIteration) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTransitionNode> transition{
      "Transition", assign<&RecoveryTransitionNode::event>(id) + "=>"_kw +
                        assign<&RecoveryTransitionNode::target>(id)};
  ParserRule<RecoveryStateNode> state{
      "State", "state"_kw + assign<&RecoveryStateNode::name>(id) +
                   many(append<&RecoveryStateNode::transitions>(transition)) +
                   "end"_kw};
  ParserRule<RecoveryStateModelNode> model{
      "Model", some(append<&RecoveryStateModelNode::states>(state))};

  const std::string text = "state PowerOff\n"
                           "  switchCapacity > RedLight\n"
                           "end\n"
                           "\n"
                           "state RedLight\n"
                           "  switchCapacity => PowerOff\n"
                           "  next => GreenLight\n"
                           "end\n";
  const auto result = parseRule(model, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryStateModelNode *>(result.value);
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->states.size(), 2u) << parseDump;
  ASSERT_NE(parsedModel->states[0], nullptr) << parseDump;
  ASSERT_NE(parsedModel->states[1], nullptr) << parseDump;
  EXPECT_EQ(parsedModel->states[0]->name, "PowerOff") << parseDump;
  EXPECT_EQ(parsedModel->states[1]->name, "RedLight") << parseDump;
  ASSERT_EQ(parsedModel->states[0]->transitions.size(), 1u) << parseDump;
  ASSERT_NE(parsedModel->states[0]->transitions.front(), nullptr) << parseDump;
  EXPECT_EQ(parsedModel->states[0]->transitions.front()->event,
            "switchCapacity");
  EXPECT_EQ(parsedModel->states[0]->transitions.front()->target, "RedLight");
}

TEST(RecoveryTest,
     FirstTopLevelElementCanRecoverLongGarbageLineUsingStableInternalPrefix) {
  struct RecoveryEntityNode : pegium::AstNode {
    string name;
    vector<pointer<RecoveryFeatureNode>> features;
  };

  struct RecoveryDomainModelNode : pegium::AstNode {
    vector<pointer<pegium::AstNode>> elements;
  };

  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryFeatureNode> feature{
      "Feature", option(enable_if<&RecoveryFeatureNode::many>("many"_kw)) +
                     assign<&RecoveryFeatureNode::name>(id) + ":"_kw +
                     assign<&RecoveryFeatureNode::type>(id)};
  ParserRule<RecoveryEntityNode> entity{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    many(append<&RecoveryEntityNode::features>(feature)) +
                    "}"_kw};
  ParserRule<RecoveryDomainModelNode> domainModel{
      "DomainModel", some(append<&RecoveryDomainModelNode::elements>(entity))};

  const std::string text = "entity Blog {\n"
                           "  <<<<<<<<<<<<<<<<<<<<<<<<\n"
                           "  title: String\n"
                           "}\n";
  const auto result = parseRule(domainModel, text, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Incomplete;
      }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryDomainModelNode *>(result.value);
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->elements.size(), 1u) << parseDump;
  auto *blog =
      dynamic_cast<RecoveryEntityNode *>(parsedModel->elements.front());
  ASSERT_NE(blog, nullptr) << parseDump;
  EXPECT_EQ(blog->name, "Blog") << parseDump;
  ASSERT_EQ(blog->features.size(), 1u) << parseDump;
  ASSERT_NE(blog->features.front(), nullptr) << parseDump;
  EXPECT_EQ(blog->features.front()->name, "title") << parseDump;
  EXPECT_EQ(blog->features.front()->type, "String") << parseDump;
}

TEST(RecoveryTest,
     TopLevelChoicePrefersFullKeywordRepairOverShorterAlternativeRewrite) {
  struct RecoveryEntityNode : pegium::AstNode {
    string name;
  };

  struct RecoveryTypeNode : pegium::AstNode {
    string name;
  };

  struct RecoveryDomainModelNode : pegium::AstNode {
    vector<pointer<pegium::AstNode>> elements;
  };

  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTypeNode> dataType{
      "DataType", "datatype"_kw + assign<&RecoveryTypeNode::name>(id)};
  ParserRule<RecoveryEntityNode> entity{
      "Entity", "entity"_kw + assign<&RecoveryEntityNode::name>(id) + "{"_kw +
                    "}"_kw};
  ParserRule<pegium::AstNode> type{"Type", dataType | entity};
  ParserRule<RecoveryDomainModelNode> domainModel{
      "DomainModel", some(append<&RecoveryDomainModelNode::elements>(type))};

  const auto result = parseRule(domainModel, "entit Blog {}", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryDomainModelNode *>(result.value);
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->elements.size(), 1u) << parseDump;
  auto *parsedEntity =
      dynamic_cast<RecoveryEntityNode *>(parsedModel->elements.front());
  ASSERT_NE(parsedEntity, nullptr) << parseDump;
  EXPECT_EQ(parsedEntity->name, "Blog");
}

TEST(RecoveryTest, LegacyOrderedChoiceOperatorRecoveryDoesNotThrow) {
  struct LegacyRecoveryParser final : PegiumParser {
    using PegiumParser::parse;

    TerminalRule<> ws{"WS", some(s)};
    Skipper skipper = SkipperBuilder().ignore(ws).build();

    TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
    TerminalRule<int> number{"NUMBER", some(d)};

    Rule<RecoveryExpression> legacyPrimary{
        "LegacyPrimary",
        create<RecoveryNumberExpression>() +
                assign<&RecoveryNumberExpression::value>(number) |
            create<RecoveryReferenceExpression>() +
                assign<&RecoveryReferenceExpression::name>(id)};
    Rule<RecoveryExpression> legacyExpression{
        "LegacyExpression",
        legacyPrimary +
            many(nest<&RecoveryBinaryExpression::left>() +
                 assign<&RecoveryBinaryExpression::op>("+"_kw | "-"_kw |
                                                       "*"_kw) +
                 assign<&RecoveryBinaryExpression::right>(legacyPrimary))};
    Rule<RecoveryExpressionEvaluation> evaluation{
        "Evaluation", "legacy"_kw +
                          assign<&RecoveryExpressionEvaluation::expression>(
                              legacyExpression) +
                          ";"_kw};

    [[nodiscard]] const pegium::grammar::ParserRule &
    getEntryRule() const noexcept override {
      return evaluation;
    }

    [[nodiscard]] const Skipper &getSkipper() const noexcept override {
      return skipper;
    }
  };

  LegacyRecoveryParser parser;

  for (const auto *text : {
           "legacy 1 + ;",
           "legacy 1 * ;",
           "legacy a + ;",
           "legacy 1 + * 2;",
           "legacy a * / b;",
           "legacy 1 +\n",
       }) {
    SCOPED_TRACE(text);
    EXPECT_NO_THROW({
      const auto result = parser.parse(text);
      EXPECT_GT(result.parsedLength, 0u);
    });
  }
}
