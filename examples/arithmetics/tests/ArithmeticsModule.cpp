#include <gtest/gtest.h>

#include <algorithm>

#include <arithmetics/services/Module.hpp>

#include <lsp/json/json.h>

#include "../src/lsp/ArithmeticsCodeActionProvider.hpp"
#include "../src/lsp/ArithmeticsFormatter.hpp"
#include "../src/references/ArithmeticsScopeProvider.hpp"

#include <pegium/ExampleTestSupport.hpp>
#include <pegium/lsp/JsonValue.hpp>
#include <pegium/services/Services.hpp>

namespace arithmetics::tests {
namespace {

constexpr std::string_view kDefaultCodeActionsKey = "pegiumDefaultCodeActions";

std::string apply_text_edits(const pegium::workspace::Document &document,
                             std::vector<::lsp::TextEdit> edits) {
  auto text = document.text();
  std::sort(edits.begin(), edits.end(),
            [&document](const auto &left, const auto &right) {
              return document.positionToOffset(left.range.start) >
                     document.positionToOffset(right.range.start);
            });
  for (const auto &edit : edits) {
    const auto begin = document.positionToOffset(edit.range.start);
    const auto end = document.positionToOffset(edit.range.end);
    text.replace(begin, end - begin, edit.newText);
  }
  return text;
}

::lsp::Diagnostic make_default_recovery_diagnostic() {
  pegium::services::JsonValue::Object action;
  action.emplace("kind", "quickfix");
  action.emplace("editKind", "delete");
  action.emplace("title", "Delete unexpected text");
  action.emplace("begin", static_cast<std::int64_t>(0));
  action.emplace("end", static_cast<std::int64_t>(1));
  action.emplace("newText", "");

  pegium::services::JsonValue::Array actions;
  actions.emplace_back(std::move(action));

  pegium::services::JsonValue::Object data;
  data.emplace(std::string(kDefaultCodeActionsKey), std::move(actions));

  ::lsp::Diagnostic diagnostic{};
  diagnostic.message = "recovery";
  diagnostic.data = pegium::lsp::to_lsp_any(
      pegium::services::JsonValue(std::move(data)));
  return diagnostic;
}

TEST(ArithmeticsModuleTest, InstallsLanguageSpecificOverrides) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  EXPECT_NE(dynamic_cast<arithmetics::services::references::ArithmeticsScopeProvider *>(
                languageServices->references.scopeProvider.get()),
            nullptr);
  EXPECT_NE(dynamic_cast<arithmetics::services::lsp::ArithmeticsCodeActionProvider *>(
                languageServices->lsp.codeActionProvider.get()),
            nullptr);
  EXPECT_NE(dynamic_cast<arithmetics::services::lsp::ArithmeticsFormatter *>(
                languageServices->lsp.formatter.get()),
            nullptr);
}

TEST(ArithmeticsModuleTest, RegistersLanguagesAndPublishesValidationDiagnostic) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(arithmetics::services::register_language_services(*shared));
  EXPECT_NE(shared->serviceRegistry->getServicesByLanguageId("arithmetics"),
            nullptr);
  EXPECT_NE(shared->serviceRegistry->getServicesByLanguageId("calc"), nullptr);

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-module.calc"),
      "arithmetics",
      "module Demo\n"
      "def value: 1 / 0;\n");

  ASSERT_NE(document, nullptr);
  EXPECT_TRUE(pegium::test::has_diagnostic_message(*document, "Division by zero"));
}

TEST(ArithmeticsModuleTest, DoesNotWarnWhenDefinitionIsAlreadyAConstantLiteral) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(arithmetics::services::register_language_services(*shared));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-constant-literal.calc"),
      "arithmetics",
      "module Demo\n"
      "def a: 5;\n");

  ASSERT_NE(document, nullptr);

  const auto hasNormalizationDiagnostic = std::ranges::any_of(
      document->diagnostics, [](const auto &diagnostic) {
        return diagnostic.code.has_value() &&
               std::holds_alternative<std::string>(*diagnostic.code) &&
               std::get<std::string>(*diagnostic.code) ==
                   "arithmetics.expression-normalizable";
      });
  EXPECT_FALSE(hasNormalizationDiagnostic);
}

TEST(ArithmeticsModuleTest, InitializeResultSerializesForLspTransport) {
  auto shared = pegium::test::make_shared_services();
  ASSERT_TRUE(arithmetics::services::register_language_services(*shared));
  ASSERT_NE(shared->lsp.languageServer, nullptr);

  ::lsp::InitializeParams params{};
  params.rootUri = ::lsp::DocumentUri(
      ::lsp::Uri::parse(pegium::test::make_file_uri("arithmetics-root")));
  params.workspaceFolders = ::lsp::Array<::lsp::WorkspaceFolder>{};

  auto result = shared->lsp.languageServer->initialize(params);

  EXPECT_NO_THROW({
    const auto json = ::lsp::toJson(std::move(result));
    EXPECT_FALSE(json.isNull());
  });
}

TEST(ArithmeticsModuleTest, FormatterFormatsCompactModule) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-format.calc"),
      "arithmetics",
      "module demo def value:1+2;def root(x,y):x^(1/y);root(value,3);");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("arithmetics");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "module demo\n"
            "def value: 1 + 2;\n"
            "def root(x, y): x ^ (1 / y);\n"
            "root(value, 3);");
}

TEST(ArithmeticsModuleTest, FormatterNormalizesBinaryOperatorSpacing) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-operators.calc"),
      "arithmetics",
      "module demo def value:2 +          8 * 7 / 0 -             9 * 7;");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("arithmetics");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "module demo\n"
            "def value: 2 + 8 * 7 / 0 - 9 * 7;");
}

TEST(ArithmeticsModuleTest, FormatterNormalizesRootIndentationWithLeadingComments) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-root-indent.calc"),
      "arithmetics",
      "            /** module comment */\n"
      "        Module basicMath\n"
      "\n"
      " \n"
      "/** test */\n"
      "def c: 2 * 5;\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("arithmetics");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "/** module comment */\n"
            "Module basicMath\n"
            "\n"
            " \n"
            "/** test */\n"
            "def c: 2 * 5;\n");
}

TEST(ArithmeticsModuleTest, CodeActionsKeepRecoveryAndLanguageSpecificFixes) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-code-actions.calc"),
      "arithmetics", "1+2;");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("arithmetics");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.codeActionProvider, nullptr);

  ::lsp::CodeActionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.range.start = document->offsetToPosition(0);
  params.range.end = document->offsetToPosition(3);
  params.context.diagnostics.push_back(make_default_recovery_diagnostic());

  ::lsp::Diagnostic normalization{};
  normalization.code = ::lsp::OneOf<int, ::lsp::String>(
      ::lsp::String("arithmetics.expression-normalizable"));
  ::lsp::LSPObject normalizationData{};
  normalizationData["constant"] = ::lsp::LSPAny(3.0);
  normalization.data = std::move(normalizationData);
  params.context.diagnostics.push_back(std::move(normalization));

  const auto actions = services->lsp.codeActionProvider->getCodeActions(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_TRUE(actions.has_value());
  ASSERT_EQ(actions->size(), 2u);
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[0]).title,
            "Delete unexpected text");
  EXPECT_EQ(std::get<::lsp::CodeAction>((*actions)[1]).title,
            "Replace with constant 3");
}

TEST(ArithmeticsModuleTest, FormatterNormalizesDocCommentsAndTags) {
  auto shared = pegium::test::make_shared_services();
  auto languageServices =
      arithmetics::services::create_language_services(*shared, "arithmetics");

  ASSERT_NE(languageServices, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(languageServices)));

  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("arithmetics-doc-comment.calc"),
      "arithmetics",
      "module demo\n"
      "/**\n"
      "*   Adds   numbers.\n"
      "* @param   x   first   value\n"
      "*   more   details\n"
      "*/\n"
      "def add(x):x+1;");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("arithmetics");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const pegium::services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.formatter, nullptr);

  ::lsp::DocumentFormattingParams params{};
  params.options.insertSpaces = true;
  params.options.tabSize = 2;
  const auto edits = services->lsp.formatter->formatDocument(
      *document, params, pegium::utils::default_cancel_token);

  ASSERT_FALSE(edits.empty());
  EXPECT_EQ(apply_text_edits(*document, edits),
            "module demo\n"
            "/**\n"
            " * Adds numbers.\n"
            " * @param x first value\n"
            " *  more details\n"
            " */\n"
            "def add(x): x + 1;");
}

} // namespace
} // namespace arithmetics::tests
