#include "RecoveryTestSupport.hpp"

using namespace pegium::parser;
using namespace pegium::test::recovery;

TEST(RecoveryTest,
     ArithmeticShapedEvaluationKeepsBrokenCallLocalWithTrailingComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primaryExpression,
                       LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryNode>() + "("_kw + assign<&RecoveryNode::token>(id) +
              ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};

  const auto result = parseRule(evaluation, "Root(64 3/0); // 4\n", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     StatementListKeepsFunctionNameForFollowingBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module, "xx;\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_FALSE(parsedModule->statements.empty()) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     StatementChoiceKeepsFunctionNameForFollowingBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "def a:1;\nxx;\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     ZeroMinStatementChoiceKeepsFunctionNameForBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.front().get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(evaluationNode->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     ZeroMinStatementChoiceKeepsFunctionNameForBrokenInfixArgumentCallWithTrailingComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0); // 4\n", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     RecursiveArgumentExpressionKeepsFunctionNameWhenCommaIsMissing) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary", create<RecoveryReferenceExpression>() +
                     assign<&RecoveryReferenceExpression::name>(id)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression", primary, LeftAssociation("/"_kw)};
  expression = binaryExpression;
  primary =
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id) |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number);
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module, "module m\nRoot(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 1u) << parseDump;
  auto *evaluationNode = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.front().get());
  ASSERT_NE(evaluationNode, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(evaluationNode->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedEvaluationTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module m\n"
                                "Root(64,3);\n"
                                "Root(64,3)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 5u) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3);\n"
                                "Root(64,3)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81)\n"
                                "Sqrt(81);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 6u) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81) // 9\n"
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 6u) << parseDump;
}

TEST(RecoveryTest,
     LongMissingSemicolonInsertStaysInsertOnlyAcrossRepeatedStatementChoiceTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module m\n"
                                "def a:1;\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
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
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;
  EXPECT_LT(result.result.recoveryReport.recoveryAttemptRuns, 512u) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  EXPECT_GE(parsedModule->statements.size(), 18u) << parseDump;
}

TEST(RecoveryTest,
     LongMissingSemicolonInsertStaysInsertOnlyAcrossRepeatedEvaluationTailWithHiddenComments) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(expression) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(expression)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(evaluation))};

  const auto result = parseRule(module,
                                "module m\n"
                                "Root(64,3); // 4\n"
                                "Root(64,3) // 4\n"
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
                                "Sqrt(81); // 9\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_LT(result.result.recoveryReport.recoveryAttemptRuns, 1024u) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  EXPECT_GE(parsedModule->statements.size(), 17u) << parseDump;
}

TEST(RecoveryTest,
     StatementChoiceWithHiddenCommentKeepsFunctionNameForBrokenInfixArgumentCall) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argumentPrimary{
      "ArgumentPrimary", create<RecoveryNumberExpression>() +
                             assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      argumentExpression{"ArgumentExpression", argumentPrimary,
                         LeftAssociation("/"_kw)};
  ParserRule<RecoveryExpression> argumentExpressionRule{
      "ArgumentExpression", argumentExpression};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option("("_kw +
                     append<&RecoveryFunctionCall::args>(argumentExpressionRule) +
                     many(","_kw +
                          append<&RecoveryFunctionCall::args>(
                              argumentExpressionRule)) +
                     ")"_kw) |
          create<RecoveryReferenceExpression>() +
              assign<&RecoveryReferenceExpression::name>(id) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", some(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "def a:1;\n"
                                "xx;\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *lastCall =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(lastCall, nullptr) << parseDump;
  EXPECT_EQ(lastCall->name, "Root") << parseDump;
}

TEST(RecoveryTest,
     OptionalStartedHeaderRecoversUnexpectedColonBeforeRequiredString) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryContact> contact{
      "Contact", "contact"_kw + ":"_kw + assign<&RecoveryContact::name>(text)};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      option(assign<&RecoveryRequirementModelNode::contact>(contact)) +
          many(append<&RecoveryRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryRequirementModelNode::requirements>(
              requirement))};

  const auto result = parseRule(model,
                                "contact:: \"team\"\n"
                                "environment prod: \"Production\"\n"
                                "req login \"Users can login\"\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_NE(parsedModel->contact, nullptr) << parseDump;
  EXPECT_EQ(parsedModel->contact->name, "\"team\"");
  ASSERT_EQ(parsedModel->environments.size(), 1u) << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
}

TEST(RecoveryTest,
     OptionalStartedHeadersKeepFollowingHeadersRecoverableAcrossWindows) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryContact> contact{
      "Contact", "contact"_kw + ":"_kw + assign<&RecoveryContact::name>(text)};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      option(assign<&RecoveryRequirementModelNode::contact>(contact)) +
          many(append<&RecoveryRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryRequirementModelNode::requirements>(
              requirement))};

  const std::string input = "contact:: \"team\"\n"
                            "environment prod \"Production\"\n"
                            "environment staging:: \"Staging\"\n"
                            "req login \"Users can login\"\n";
  const auto result = parseRule(model, input, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_NE(parsedModel->contact, nullptr) << parseDump;
  EXPECT_EQ(parsedModel->contact->name, "\"team\"") << parseDump;
  ASSERT_EQ(parsedModel->environments.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->environments[0]->name, "prod") << parseDump;
  EXPECT_EQ(parsedModel->environments[1]->name, "staging") << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->name, "login") << parseDump;
}

TEST(RecoveryTest,
     LongGarbagePrefixBeforeFirstRequirementCanDeleteScanIntoEntryRule) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id) +
                         assign<&RecoveryRequirementNode::label>(text)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      some(append<&RecoveryRequirementModelNode::requirements>(requirement))};

  const std::string input = "<<<<<<<<<<<<<<<<<<<<<<<<\n"
                            "req login \"Users can login\"\n";
  const auto result = parseRule(model, input, skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_FALSE(std::ranges::any_of(result.parseDiagnostics,
                                   [](const auto &diagnostic) {
                                     return diagnostic.kind ==
                                            ParseDiagnosticKind::Incomplete;
                                   }))
      << parseDump;

  const auto garbageBegin = input.find("<<<<<<<<<<<<<<<<<<<<<<<<");
  ASSERT_NE(garbageBegin, std::string::npos);
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics,
      [garbageBegin](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.beginOffset ==
                   static_cast<pegium::TextOffset>(garbageBegin);
      }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->name, "login") << parseDump;
}

TEST(RecoveryTest, OptionalTailListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryNameListNode>() +
          option("tail"_kw + append<&RecoveryNameListNode::names>(id) +
                 many(","_kw + append<&RecoveryNameListNode::names>(id)))};

  const auto result = parseRule(model, "root tail alpha beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryNameListNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->names.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->names[0], "alpha");
  EXPECT_EQ(parsedModel->names[1], "beta");
}

TEST(RecoveryTest,
     ZeroMinTopLevelRepetitionDoesNotStopBeforeRecoverableDefinitionKeyword) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> reference{
      "Reference",
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryDefinition> definition{
      "Definition", "def"_kw + create<RecoveryDefinition>() +
                        assign<&RecoveryDefinition::name>(id) + ":"_kw +
                        assign<&RecoveryDefinition::value>(number) + ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      create<RecoveryExpressionEvaluation>() +
          assign<&RecoveryExpressionEvaluation::expression>(reference) +
          ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "de a: 5;\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;

  // Under the 4-axis recovery ranking (Phase C) the comparator systematically
  // prefers later-first-edit candidates. Here the "insert `;` then delete the
  // unexpected `: 5`" interpretation (first edit at offset 21) beats the
  // "fuzzy-replace `de` with `def`" interpretation (first edit at 18). The
  // top-level zero-min repetition still resumes past the malformed input
  // instead of stopping early, which is the invariant this test protects.
  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  EXPECT_GE(parsedModule->statements.size(), 1u) << parseDump;
}

TEST(RecoveryTest, OptionalTailListRecoversExtraCommaBeforeItem) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryNameListNode>() +
          option("tail"_kw + append<&RecoveryNameListNode::names>(id) +
                 many(","_kw + append<&RecoveryNameListNode::names>(id)))};

  const auto result = parseRule(model, "root tail alpha,, beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }))
      << parseDump;

  auto *parsedModel = dynamic_cast<RecoveryNameListNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->names.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->names[0], "alpha");
  EXPECT_EQ(parsedModel->names[1], "beta");
}

TEST(RecoveryTest, LongPrefixOptionalTailListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryPrefixedNameListNode> model{
      "Model",
      "root"_kw + create<RecoveryPrefixedNameListNode>() +
          assign<&RecoveryPrefixedNameListNode::name>(id) +
          assign<&RecoveryPrefixedNameListNode::label>(text) +
          option(
              "tail"_kw + append<&RecoveryPrefixedNameListNode::names>(id) +
              many(","_kw + append<&RecoveryPrefixedNameListNode::names>(id)))};

  const auto result = parseRule(
      model, "root login \"Users can login\" tail alpha beta", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
}

TEST(RecoveryTest, DirectApplicableListRecoversMissingCommaBetweenItems) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryTaggedRequirementListNode> applicable{
      "Applicable",
      "applicable"_kw + "for"_kw + create<RecoveryTaggedRequirementListNode>() +
          append<&RecoveryTaggedRequirementListNode::environments>(id) +
          many(","_kw +
               append<&RecoveryTaggedRequirementListNode::environments>(id))};

  const auto result =
      parseRule(applicable, "applicable for prod staging", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;
}

TEST(RecoveryTest,
     RequirementModelRecoversMissingCommaInsideOptionalApplicableTail) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  ParserRule<RecoveryEnvironmentNode> environment{
      "Environment", "environment"_kw + create<RecoveryEnvironmentNode>() +
                         assign<&RecoveryEnvironmentNode::name>(id) + ":"_kw +
                         assign<&RecoveryEnvironmentNode::label>(text)};
  ParserRule<RecoveryTaggedRequirementListNode> requirement{
      "Requirement",
      "req"_kw + create<RecoveryTaggedRequirementListNode>() +
          assign<&RecoveryTaggedRequirementListNode::name>(id) +
          assign<&RecoveryTaggedRequirementListNode::label>(text) +
          option("applicable"_kw + "for"_kw +
                 append<&RecoveryTaggedRequirementListNode::environments>(id) +
                 many(","_kw +
                      append<&RecoveryTaggedRequirementListNode::environments>(
                          id)))};
  ParserRule<RecoveryTaggedRequirementModelNode> model{
      "Model",
      create<RecoveryTaggedRequirementModelNode>() +
          many(append<&RecoveryTaggedRequirementModelNode::environments>(
              environment)) +
          some(append<&RecoveryTaggedRequirementModelNode::requirements>(
              requirement))};

  const auto result =
      parseRule(model,
                "environment prod: \"Production\"\n"
                "environment staging: \"Staging\"\n"
                "req login \"Users can login\" applicable for prod staging",
                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(result.result.recoveryReport.hasRecovered) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(result.parseDiagnostics,
                                  [](const ParseDiagnostic &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }))
      << parseDump;

  auto *parsedModel =
      dynamic_cast<RecoveryTaggedRequirementModelNode *>(result.value.get());
  ASSERT_NE(parsedModel, nullptr);
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  ASSERT_NE(parsedModel->requirements[0], nullptr);
  ASSERT_EQ(parsedModel->requirements[0]->environments.size(), 2u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->environments[0], "prod") << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->environments[1], "staging")
      << parseDump;
}

