#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>

#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

namespace arithmetics::tests {
namespace {

std::string summarize_expression(const ast::Expression *expression) {
  if (expression == nullptr) {
    return "<null-expr>";
  }
  if (const auto *number = dynamic_cast<const ast::NumberLiteral *>(expression);
      number != nullptr) {
    return "number:" + std::to_string(number->value);
  }
  if (const auto *call = dynamic_cast<const ast::FunctionCall *>(expression);
      call != nullptr) {
    return "call:" + call->func.getRefText();
  }
  if (const auto *grouped =
          dynamic_cast<const ast::GroupedExpression *>(expression);
      grouped != nullptr) {
    return "group(" + summarize_expression(grouped->expression.get()) + ")";
  }
  if (const auto *binary =
          dynamic_cast<const ast::BinaryExpression *>(expression);
      binary != nullptr) {
    return "binary(" + summarize_expression(binary->left.get()) + " " +
           binary->op + " " + summarize_expression(binary->right.get()) + ")";
  }
  return "other-expr";
}

std::string summarize_module_statement_shapes(const ast::Module &module) {
  std::string summary;
  for (const auto &statement : module.statements) {
    if (!summary.empty()) {
      summary += " | ";
    }
    if (const auto *definition =
            dynamic_cast<const ast::Definition *>(statement.get());
        definition != nullptr) {
      summary += "def:";
      summary += definition->name;
      continue;
    }
    if (const auto *evaluation =
            dynamic_cast<const ast::Evaluation *>(statement.get());
        evaluation != nullptr) {
      summary += "eval:";
      summary += summarize_expression(evaluation->expression.get());
      continue;
    }
    summary += "other";
  }
  return summary;
}

bool has_diagnostic_at_offset(
    std::span<const pegium::parser::ParseDiagnostic> diagnostics,
    pegium::parser::ParseDiagnosticKind kind, pegium::TextOffset offset) {
  return std::ranges::any_of(diagnostics, [&](const auto &diagnostic) {
    return diagnostic.kind == kind && diagnostic.offset == offset;
  });
}

bool has_deleted_range(
    std::span<const pegium::parser::ParseDiagnostic> diagnostics,
    pegium::TextOffset beginOffset, pegium::TextOffset endOffset) {
  return std::ranges::any_of(diagnostics, [&](const auto &diagnostic) {
    return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
           diagnostic.beginOffset == beginOffset &&
           diagnostic.endOffset == endOffset;
  });
}

bool has_expected_rule_diagnostic(
    std::span<const pegium::parser::ParseDiagnostic> diagnostics,
    pegium::parser::ParseDiagnosticKind kind, std::string_view ruleName) {
  return std::ranges::any_of(diagnostics, [&](const auto &diagnostic) {
    if (diagnostic.kind != kind || diagnostic.element == nullptr) {
      return false;
    }
    const auto *element = diagnostic.element;
    if (element->getKind() == pegium::grammar::ElementKind::Assignment) {
      element =
          static_cast<const pegium::grammar::Assignment *>(element)->getElement();
    }
    const auto *expectedRule =
        dynamic_cast<const pegium::grammar::AbstractRule *>(element);
    return expectedRule != nullptr && expectedRule->getName() == ruleName;
  });
}

void validate_sample(const pegium::test::NamedSampleFile &sample,
                     std::string_view sourceText,
                     pegium::workspace::Document &document) {
  const auto &parsed = document.parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr) << sample.label << " :: " << parseDump;

  if (sample.label == "missing_semicolon_before_next_definition.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Inserted))
        << sample.label << " :: " << parseDump;
    EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *firstDefinition =
        dynamic_cast<ast::Definition *>(module->statements[0].get());
    auto *secondDefinition =
        dynamic_cast<ast::Definition *>(module->statements[1].get());
    ASSERT_NE(firstDefinition, nullptr);
    ASSERT_NE(secondDefinition, nullptr);
    EXPECT_EQ(firstDefinition->name, "a");
    EXPECT_EQ(secondDefinition->name, "b");
    return;
  }

  if (sample.label == "missing_definition_colon.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Inserted))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *brokenDefinition =
        dynamic_cast<ast::Definition *>(module->statements[0].get());
    auto *keptDefinition =
        dynamic_cast<ast::Definition *>(module->statements[1].get());
    ASSERT_NE(brokenDefinition, nullptr);
    ASSERT_NE(keptDefinition, nullptr);
    EXPECT_EQ(brokenDefinition->name, "broken");
    EXPECT_EQ(keptDefinition->name, "kept");
    ASSERT_NE(brokenDefinition->expr, nullptr);
    EXPECT_EQ(summarize_expression(brokenDefinition->expr.get()),
              "binary(number:1.000000 + number:2.000000)");
    return;
  }

  if (sample.label == "missing_definition_close_paren.calc") {
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[0].get());
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements[1].get());
    ASSERT_NE(definition, nullptr);
    ASSERT_NE(evaluation, nullptr);
    EXPECT_EQ(definition->name, "root");
    EXPECT_EQ(definition->args.size(), 2u);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
    ASSERT_NE(call, nullptr)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    EXPECT_EQ(call->func.getRefText(), "root");
    ASSERT_EQ(call->args.size(), 2u);
    return;
  }

  if (sample.label == "missing_call_argument_comma.calc") {
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements[1].get());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
    ASSERT_NE(call, nullptr)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    EXPECT_EQ(call->func.getRefText(), "root");
    ASSERT_EQ(call->args.size(), 2u);
    return;
  }

  if (sample.label == "module_keyword_gap.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Inserted))
        << sample.label << " :: " << parseDump;
    EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    EXPECT_EQ(module->name, "basicmath");
    return;
  }

  if (sample.label == "module_keyword_missing_codepoint.calc") {
    EXPECT_TRUE(has_diagnostic_at_offset(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Replaced,
        0u))
        << sample.label << " :: " << parseDump;
    EXPECT_EQ(module->name, "basicmath");
    return;
  }

  if (sample.label == "module_keyword_missing_suffix.calc") {
    EXPECT_TRUE(has_diagnostic_at_offset(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Replaced,
        0u))
        << sample.label << " :: " << parseDump;
    EXPECT_EQ(module->name, "basicmath");
    return;
  }

  if (sample.label == "def_keyword_missing_codepoint.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Replaced))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 1u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[0].get());
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->name, "a");
    return;
  }

  if (sample.label == "unexpected_token_after_operator.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    const auto results = evaluate_module(*module);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_DOUBLE_EQ(results.front(), 16.0);
    return;
  }

  if (sample.label == "recovered_call_argument_prefix.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 3u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements.back().get());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->args.size(), 1u);
    auto *argument = dynamic_cast<ast::NumberLiteral *>(call->args.front().get());
    ASSERT_NE(argument, nullptr);
    EXPECT_DOUBLE_EQ(argument->value, 81.0);
    return;
  }

  if (sample.label == "short_missing_call_rhs.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements.back().get());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->func.getRefText(), "sqrt");
    ASSERT_EQ(call->args.size(), 1u);
    auto *argument = dynamic_cast<ast::NumberLiteral *>(call->args.front().get());
    ASSERT_NE(argument, nullptr);
    EXPECT_DOUBLE_EQ(argument->value, 81.0);
    return;
  }

  if (sample.label == "empty_call_argument_list.calc" ||
      sample.label == "empty_call_argument_list_without_semicolon.calc") {
    EXPECT_TRUE(has_expected_rule_diagnostic(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Inserted,
        "Expression"))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 3u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements.back().get());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->func.getRefText(), "sqrt");
    return;
  }

  if (sample.label == "long_delete_run_after_operator.calc") {
    const auto plusPos = sourceText.find("+++++++++");
    ASSERT_NE(plusPos, std::string_view::npos);
    EXPECT_TRUE(has_deleted_range(
        parsed.parseDiagnostics, static_cast<pegium::TextOffset>(plusPos),
        static_cast<pegium::TextOffset>(plusPos + std::string_view{"+++++++++"}.size())))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 3u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements[1].get());
    ASSERT_NE(evaluation, nullptr);
    auto *binary =
        dynamic_cast<ast::BinaryExpression *>(evaluation->expression.get());
    ASSERT_NE(binary, nullptr);
    auto *left = dynamic_cast<ast::NumberLiteral *>(binary->left.get());
    auto *right = dynamic_cast<ast::NumberLiteral *>(binary->right.get());
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(binary->op, "*");
    EXPECT_EQ(left->value, 2.0);
    EXPECT_EQ(right->value, 7.0);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[2].get());
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->name, "b");
    return;
  }

  if (sample.label == "long_delete_run_beyond_default_budget.calc") {
    constexpr std::string_view operatorRun = "+++++++++++++++++++++++++++++++++++";
    const auto plusPos = sourceText.find(operatorRun);
    ASSERT_NE(plusPos, std::string_view::npos);
    EXPECT_TRUE(has_deleted_range(
        parsed.parseDiagnostics, static_cast<pegium::TextOffset>(plusPos),
        static_cast<pegium::TextOffset>(plusPos + operatorRun.size())))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 3u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements[1].get());
    ASSERT_NE(evaluation, nullptr);
    auto *binary =
        dynamic_cast<ast::BinaryExpression *>(evaluation->expression.get());
    ASSERT_NE(binary, nullptr);
    auto *left = dynamic_cast<ast::NumberLiteral *>(binary->left.get());
    auto *right = dynamic_cast<ast::NumberLiteral *>(binary->right.get());
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(binary->op, "*");
    EXPECT_EQ(left->value, 2.0);
    EXPECT_EQ(right->value, 7.0);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[2].get());
    ASSERT_NE(definition, nullptr);
    EXPECT_EQ(definition->name, "b");
    return;
  }

  if (sample.label == "long_plus_run.calc") {
    const auto plusPos = sourceText.find('+');
    const auto semicolonPos = sourceText.find(';', plusPos);
    ASSERT_NE(plusPos, std::string_view::npos);
    ASSERT_NE(semicolonPos, std::string_view::npos);
    EXPECT_TRUE(has_deleted_range(
        parsed.parseDiagnostics, static_cast<pegium::TextOffset>(plusPos),
        static_cast<pegium::TextOffset>(semicolonPos)))
        << sample.label << " :: " << parseDump;
    const auto results = evaluate_module(*module);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_DOUBLE_EQ(results.front(), 14.0);
    return;
  }

  if (sample.label == "long_garbage_tail.calc") {
    EXPECT_TRUE(pegium::test::has_parse_diagnostic_kind(
        parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Deleted))
        << sample.label << " :: " << parseDump;
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    const auto results = evaluate_module(*module);
    ASSERT_EQ(results.size(), 1u);
    EXPECT_DOUBLE_EQ(results.front(), 1.0);
  }
}

std::vector<pegium::test::NamedSampleFile> recovery_samples() {
  return pegium::test::collect_named_sample_files(
      pegium::test::current_source_directory() / "recovery-samples",
      {".calc"});
}

class ArithmeticsRecoverySampleTest
    : public ::testing::TestWithParam<pegium::test::NamedSampleFile> {};

TEST(ArithmeticsRecoverySampleCorpusTest, IsNotEmpty) {
  EXPECT_FALSE(recovery_samples().empty());
}

TEST_P(ArithmeticsRecoverySampleTest, RecoversCompletely) {
  const auto &sample = GetParam();
  parser::ArithmeticParser parser;
  const auto text = pegium::test::read_text_file(sample.path);
  auto document = pegium::test::parse_document(
      parser, text, pegium::test::make_file_uri(sample.label), "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.fullMatch) << sample.label << " :: " << parseDump;
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered)
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(parsed.parseDiagnostics.empty())
      << sample.label << " :: " << parseDump;
  EXPECT_FALSE(pegium::test::has_parse_diagnostic_kind(
      parsed.parseDiagnostics, pegium::parser::ParseDiagnosticKind::Incomplete))
      << sample.label << " :: " << parseDump;
  validate_sample(sample, text, *document);
}

INSTANTIATE_TEST_SUITE_P(
    RecoverySamples, ArithmeticsRecoverySampleTest,
    ::testing::ValuesIn(recovery_samples()),
    [](const ::testing::TestParamInfo<pegium::test::NamedSampleFile> &info) {
      return info.param.testName;
    });

} // namespace
} // namespace arithmetics::tests
