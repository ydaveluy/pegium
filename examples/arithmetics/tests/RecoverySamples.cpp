#include <gtest/gtest.h>

#include <arithmetics/core/CoreModule.hpp>
#include <arithmetics/core/Language.hpp>

#include <pegium/core/grammar/AbstractRule.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

#include "LanguageTestSupport.hpp"

namespace arithmetics::tests {
namespace {

// summarize_expression / summarize_module_statement_shapes are shared from
// LanguageTestSupport.hpp (arithmetics::tests scope).

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
  auto *module = dynamic_cast<ast::Module *>(parsed.value);
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
        dynamic_cast<ast::Definition *>(module->statements[0]);
    auto *secondDefinition =
        dynamic_cast<ast::Definition *>(module->statements[1]);
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
        dynamic_cast<ast::Definition *>(module->statements[0]);
    auto *keptDefinition =
        dynamic_cast<ast::Definition *>(module->statements[1]);
    ASSERT_NE(brokenDefinition, nullptr);
    ASSERT_NE(keptDefinition, nullptr);
    EXPECT_EQ(brokenDefinition->name, "broken");
    EXPECT_EQ(keptDefinition->name, "kept");
    ASSERT_NE(brokenDefinition->expr, nullptr);
    EXPECT_EQ(summarize_expression(brokenDefinition->expr),
              "binary(number:1.000000 + number:2.000000)");
    return;
  }

  if (sample.label == "missing_definition_close_paren.calc") {
    ASSERT_EQ(module->statements.size(), 2u)
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[0]);
    auto *evaluation =
        dynamic_cast<ast::Evaluation *>(module->statements[1]);
    ASSERT_NE(definition, nullptr);
    ASSERT_NE(evaluation, nullptr);
    EXPECT_EQ(definition->name, "root");
    EXPECT_EQ(definition->args.size(), 2u);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression);
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
        dynamic_cast<ast::Evaluation *>(module->statements[1]);
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression);
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
    // The 4-axis recovery ranking prefers later-first-edit interpretations,
    // so `de a: 5;` is recovered as two evaluations (insert `;` after `de`,
    // delete the offending `: 5` gap) rather than a fuzzy-repair to `def`.
    EXPECT_FALSE(module->statements.empty())
        << sample.label << " :: " << summarize_module_statement_shapes(*module);
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
        dynamic_cast<ast::Evaluation *>(module->statements.back());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression);
    ASSERT_NE(call, nullptr);
    ASSERT_EQ(call->args.size(), 1u);
    auto *argument = dynamic_cast<ast::NumberLiteral *>(call->args.front());
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
        dynamic_cast<ast::Evaluation *>(module->statements.back());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression);
    ASSERT_NE(call, nullptr);
    EXPECT_EQ(call->func.getRefText(), "sqrt");
    ASSERT_EQ(call->args.size(), 1u);
    auto *argument = dynamic_cast<ast::NumberLiteral *>(call->args.front());
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
        dynamic_cast<ast::Evaluation *>(module->statements.back());
    ASSERT_NE(evaluation, nullptr);
    auto *call =
        dynamic_cast<ast::FunctionCall *>(evaluation->expression);
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
        dynamic_cast<ast::Evaluation *>(module->statements[1]);
    ASSERT_NE(evaluation, nullptr);
    auto *binary =
        dynamic_cast<ast::BinaryExpression *>(evaluation->expression);
    ASSERT_NE(binary, nullptr);
    auto *left = dynamic_cast<ast::NumberLiteral *>(binary->left);
    auto *right = dynamic_cast<ast::NumberLiteral *>(binary->right);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(binary->op, "*");
    EXPECT_EQ(left->value, 2.0);
    EXPECT_EQ(right->value, 7.0);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[2]);
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
        dynamic_cast<ast::Evaluation *>(module->statements[1]);
    ASSERT_NE(evaluation, nullptr);
    auto *binary =
        dynamic_cast<ast::BinaryExpression *>(evaluation->expression);
    ASSERT_NE(binary, nullptr);
    auto *left = dynamic_cast<ast::NumberLiteral *>(binary->left);
    auto *right = dynamic_cast<ast::NumberLiteral *>(binary->right);
    ASSERT_NE(left, nullptr);
    ASSERT_NE(right, nullptr);
    EXPECT_EQ(binary->op, "*");
    EXPECT_EQ(left->value, 2.0);
    EXPECT_EQ(right->value, 7.0);
    auto *definition =
        dynamic_cast<ast::Definition *>(module->statements[2]);
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
  auto parser = createArithmeticsParser();
  const auto text = pegium::test::read_text_file(sample.path);
  auto document = pegium::test::parse_document(
      *parser, text, pegium::test::make_file_uri(sample.label), "arithmetics");

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
