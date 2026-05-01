#include "RecoveryTestSupport.hpp"

using namespace pegium::parser;
using namespace pegium::test::recovery;

TEST(RecoveryTest,
     MissingDelimiterInsertStillWinsInsideModuleWithNonCompetingStatementChoice) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryEvaluation> impossible{
      "Impossible", "value"_kw + assign<&RecoveryEvaluation::name>(id) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | impossible};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a 5;\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStillWinsInsideModuleWithStatementChoice) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertStillWinsInsideModuleWithDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\ndef c: b;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 3u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
}

TEST(RecoveryTest,
     MissingColonInsertionsStayLocalAcrossLaterRecoveryWindowsInDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result = parseRule(module, "module m\ndef a 5;\ndef b 3;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  const auto insertedColonCount = std::ranges::count_if(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset;
      });
  EXPECT_EQ(insertedColonCount, 2) << parseDump;
  EXPECT_TRUE(std::ranges::all_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        if (!isSyntaxParseDiagnostic(diagnostic.kind)) {
          return true;
        }
        return diagnostic.kind == ParseDiagnosticKind::Inserted &&
               diagnostic.element != nullptr &&
               diagnostic.element->getKind() ==
                   pegium::grammar::ElementKind::Literal &&
               diagnostic.beginOffset == diagnostic.endOffset;
      }))
      << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;

  auto *firstNumber =
      dynamic_cast<RecoveryNumberExpression *>(firstDefinition->expr.get());
  auto *secondNumber =
      dynamic_cast<RecoveryNumberExpression *>(secondDefinition->expr.get());
  ASSERT_NE(firstNumber, nullptr) << parseDump;
  ASSERT_NE(secondNumber, nullptr) << parseDump;
  EXPECT_EQ(firstNumber->value, 5) << parseDump;
  EXPECT_EQ(secondNumber->value, 3) << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertKeepsFollowingDefinitionInsideStatementChoiceBeforeTrailingGarbage) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", definition | evaluation};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(statement))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\n;\n;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);
  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
}

TEST(RecoveryTest,
     MissingSemicolonInsertKeepsFollowingDefinitionsBeforeTrailingGarbageInDefinitionRepetition) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                            assign<&RecoveryNumberExpression::value>(number) |
                        create<RecoveryReferenceExpression>() +
                            assign<&RecoveryReferenceExpression::name>(id)};
  ParserRule<RecoveryParameter> parameter{"Parameter",
                                          assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryModule> module{
      "Module", "module"_kw + assign<&RecoveryModule::name>(id) +
                    many(append<&RecoveryModule::statements>(definition))};

  const auto result =
      parseRule(module, "module m\ndef a: 5\ndef b: 3;\n;\n;", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 2u) << parseDump;

  auto *firstDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[0].get());
  auto *secondDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr) << parseDump;
  ASSERT_NE(secondDefinition, nullptr) << parseDump;
  EXPECT_EQ(firstDefinition->name, "a") << parseDump;
  EXPECT_EQ(secondDefinition->name, "b") << parseDump;
}

TEST(RecoveryTest, StartedOptionalBranchKeepsRecoveredArgumentPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "sqrt(81/);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(
      result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Deleted &&
               diagnostic.offset == 7u;
      }))
      << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "sqrt") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 1u) << parseDump;

  auto *parsedArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args.front().get());
  ASSERT_NE(parsedArgument, nullptr) << parseDump;
  EXPECT_EQ(parsedArgument->value, 81) << parseDump;
}

TEST(RecoveryTest, StartedOptionalBranchWithEmptyCallKeepsCallPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "sqrt();", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "sqrt") << parseDump;
  EXPECT_TRUE(parsedCall->args.empty()) << parseDump;
}

TEST(RecoveryTest, StartedOptionalBranchWithMissingCommaKeepsCallPrefix) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryExpression> argument{
      "Argument", create<RecoveryNumberExpression>() +
                      assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primary{
      "Primary",
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
              option(
                  "("_kw + append<&RecoveryFunctionCall::args>(argument) +
                  many(","_kw + append<&RecoveryFunctionCall::args>(argument)) +
                  ")"_kw) |
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "root(64 3);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "root") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 2u) << parseDump;

  auto *firstArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args[0].get());
  auto *secondArgument =
      dynamic_cast<RecoveryNumberExpression *>(parsedCall->args[1].get());
  ASSERT_NE(firstArgument, nullptr) << parseDump;
  ASSERT_NE(secondArgument, nullptr) << parseDump;
  EXPECT_EQ(firstArgument->value, 64) << parseDump;
  EXPECT_EQ(secondArgument->value, 3) << parseDump;
}

TEST(RecoveryTest, InfixArgumentCallKeepsFunctionNameWhenCommaIsMissing) {
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
          create<RecoveryNumberExpression>() +
              assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(primary) + ";"_kw};

  const auto result = parseRule(evaluation, "root(64 3/0);", skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  auto *parsedEvaluation =
      dynamic_cast<RecoveryExpressionEvaluation *>(result.value.get());
  ASSERT_NE(parsedEvaluation, nullptr) << parseDump;
  auto *parsedCall =
      dynamic_cast<RecoveryFunctionCall *>(parsedEvaluation->expression.get());
  ASSERT_NE(parsedCall, nullptr) << parseDump;
  EXPECT_EQ(parsedCall->name, "root") << parseDump;
  ASSERT_EQ(parsedCall->args.size(), 2u) << parseDump;
}

TEST(RecoveryTest, ArithmeticShapedModuleKeepsBrokenCallLocal) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
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
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};
  const std::string input =
      "module basicMath\n"
      "\n"
      "Root(64 3/0); // 4\n";

  const auto result = parseRule(module, input, skipper);
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
  ASSERT_EQ(call->args.size(), 2u) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsComplexBrokenCallLocalAfterSplitGarbageBlock) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryGroupedExpression>() + "("_kw +
          assign<&RecoveryGroupedExpression::expression>(expression) + ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "xx\n"
                                "xxxxxxxxxxxx;\n"
                                "xx\n"
                                "\n"
                                "Root(64 + 5 3/0+5-3); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_LT(result.result.recoveryReport.recoveryAttemptRuns, 512u) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_FALSE(parsedModule->statements.empty()) << parseDump;
  auto *lastEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements.back().get());
  ASSERT_NE(lastEvaluation, nullptr) << parseDump;
  auto *call =
      dynamic_cast<RecoveryFunctionCall *>(lastEvaluation->expression.get());
  ASSERT_NE(call, nullptr) << parseDump;
  EXPECT_EQ(call->name, "Root") << parseDump;
  EXPECT_FALSE(call->args.empty()) << parseDump;
}

TEST(RecoveryTest, ArithmeticShapedModuleKeepsBrokenCallLocalWithoutComment) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
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
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsTrailingFunctionDefinitionsAfterRecoveredSemicolons) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryGroupedExpression>() + "("_kw +
          assign<&RecoveryGroupedExpression::expression>(expression) + ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
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
                                "    root(x, 2);\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_LT(result.result.recoveryReport.recoveryAttemptRuns, 1024u) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 6u) << parseDump;

  auto *rootDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[4].get());
  auto *sqrtDefinition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[5].get());
  ASSERT_NE(rootDefinition, nullptr) << parseDump;
  ASSERT_NE(sqrtDefinition, nullptr) << parseDump;
  EXPECT_EQ(rootDefinition->name, "root") << parseDump;
  EXPECT_EQ(sqrtDefinition->name, "sqrt") << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsTrailingEvaluationsAfterMixedDefinitionRecovery) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
  ParserRule<RecoveryExpression> expression{
      "Expression", create<RecoveryNumberExpression>() +
                        assign<&RecoveryNumberExpression::value>(number)};
  ParserRule<RecoveryExpression> primaryExpression{
      "PrimaryExpression", create<RecoveryNumberExpression>() +
                               assign<&RecoveryNumberExpression::value>(number)};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op,
            &RecoveryBinaryExpression::right>
      binaryExpression{"BinaryExpression",
                       primaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};
  expression = binaryExpression;
  primaryExpression =
      create<RecoveryGroupedExpression>() + "("_kw +
          assign<&RecoveryGroupedExpression::expression>(expression) + ")"_kw |
      create<RecoveryNumberExpression>() +
          assign<&RecoveryNumberExpression::value>(number) |
      create<RecoveryFunctionCall>() + assign<&RecoveryFunctionCall::name>(id) +
          option("("_kw +
                 append<&RecoveryFunctionCall::args>(expression) +
                 many(","_kw + append<&RecoveryFunctionCall::args>(expression)) +
                 ")"_kw);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
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
                                "b % 2; // 1\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
  EXPECT_LT(result.result.recoveryReport.recoveryAttemptRuns, 1024u) << parseDump;

  auto *parsedModule = dynamic_cast<RecoveryModule *>(result.value.get());
  ASSERT_NE(parsedModule, nullptr) << parseDump;
  ASSERT_EQ(parsedModule->statements.size(), 10u) << parseDump;

  auto *b1Definition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[2].get());
  auto *b2Definition = dynamic_cast<RecoveryDefinitionWithOptionalArgs *>(
      parsedModule->statements[3].get());
  auto *firstEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[8].get());
  auto *secondEvaluation = dynamic_cast<RecoveryExpressionEvaluation *>(
      parsedModule->statements[9].get());
  ASSERT_NE(b1Definition, nullptr) << parseDump;
  ASSERT_NE(b2Definition, nullptr) << parseDump;
  ASSERT_NE(firstEvaluation, nullptr) << parseDump;
  ASSERT_NE(secondEvaluation, nullptr) << parseDump;
  EXPECT_EQ(b1Definition->name, "b1") << parseDump;
  EXPECT_EQ(b2Definition->name, "b2") << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsBrokenCallLocalWithTrailingCommentAndSlashOnly) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
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
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

TEST(RecoveryTest,
     ArithmeticShapedModuleKeepsBrokenCallLocalWithTrailingCommentAndReferenceBranch) {
  const auto whitespace = some(s);
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  const auto skipper = SkipperBuilder().ignore(whitespace).hide(slComment).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  ParserRule<RecoveryParameter> parameter{
      "Parameter", assign<&RecoveryParameter::name>(id)};
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
                 ")"_kw) |
      create<RecoveryReferenceExpression>() +
          assign<&RecoveryReferenceExpression::name>(id);
  ParserRule<RecoveryDefinitionWithOptionalArgs> definition{
      "Definition",
      "def"_kw + assign<&RecoveryDefinitionWithOptionalArgs::name>(id) +
          option("("_kw +
                 append<&RecoveryDefinitionWithOptionalArgs::args>(parameter) +
                 many(","_kw +
                      append<&RecoveryDefinitionWithOptionalArgs::args>(
                          parameter)) +
                 ")"_kw) +
          ":"_kw +
          assign<&RecoveryDefinitionWithOptionalArgs::expr>(expression) +
          ";"_kw};
  ParserRule<RecoveryExpressionEvaluation> evaluation{
      "Evaluation",
      assign<&RecoveryExpressionEvaluation::expression>(expression) + ";"_kw};
  ParserRule<pegium::AstNode> statement{"Statement", evaluation};
  statement = definition | evaluation;
  ParserRule<RecoveryModule> module{
      "Module",
      "module"_kw + assign<&RecoveryModule::name>(id) +
          many(append<&RecoveryModule::statements>(statement))};

  const auto result = parseRule(module,
                                "module basicMath\n"
                                "\n"
                                "Root(64 3/0); // 4\n",
                                skipper);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;
}

