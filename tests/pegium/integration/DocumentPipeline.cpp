#include <gtest/gtest.h>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <vector>

#include <arithmetics/ast.hpp>
#include <arithmetics/services/Module.hpp>

#include <pegium/ExampleTestSupport.hpp>

namespace pegium::integration {
namespace {

class DocumentPipelineIntegrationTest : public ::testing::Test {
protected:
  std::unique_ptr<services::SharedServices> shared = test::make_shared_services();

  void SetUp() override {
    ASSERT_TRUE(arithmetics::services::register_language_services(*shared));
  }

  std::shared_ptr<workspace::Document> updateDocument(std::string_view fileName,
                                                      std::string text) {
    const auto uri = test::make_file_uri(fileName);
    auto textDocument =
        shared->workspace.textDocuments->open(uri, "arithmetics", std::move(text), 1);

    shared->lsp.documentUpdateHandler->didChangeContent({.document = textDocument});

    const bool ready = test::wait_until([&]() {
      const auto documentId = shared->workspace.documents->getDocumentId(uri);
      auto document = shared->workspace.documents->getDocument(documentId);
      return document != nullptr &&
             document->state >= workspace::DocumentState::Validated;
    });
    EXPECT_TRUE(ready);
    if (!ready) {
      return nullptr;
    }

    const auto documentId = shared->workspace.documents->getDocumentId(uri);
    auto document = shared->workspace.documents->getDocument(documentId);
    EXPECT_NE(document, nullptr);
    return document;
  }

  static std::vector<const services::Diagnostic *>
  parseDiagnostics(const workspace::Document &document) {
    std::vector<const services::Diagnostic *> diagnostics;
    for (const auto &diagnostic : document.diagnostics) {
      if (diagnostic.source == "parse") {
        diagnostics.push_back(std::addressof(diagnostic));
      }
    }
    return diagnostics;
  }

  static std::size_t parseDiagnosticCount(const workspace::Document &document) {
    return std::ranges::count_if(document.diagnostics, [](const auto &diagnostic) {
      return diagnostic.source == "parse";
    });
  }

  static std::string dumpDiagnostics(
      const std::vector<const services::Diagnostic *> &diagnostics) {
    std::string dump;
    for (const auto *diagnostic : diagnostics) {
      if (!dump.empty()) {
        dump += " | ";
      }
      dump += diagnostic->message + "@" + std::to_string(diagnostic->begin) + "-" +
              std::to_string(diagnostic->end);
    }
    return dump;
  }
};

TEST_F(DocumentPipelineIntegrationTest,
       DocumentUpdateHandlerBuildsAndValidatesOpenDocument) {
  auto document = updateDocument(
      "pipeline.calc",
      "module Demo\n"
      "def value: 1 / 0;\n"
      "value;\n");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, workspace::DocumentState::Validated);
  EXPECT_TRUE(test::has_diagnostic_message(*document, "Division by zero"));
}

TEST_F(DocumentPipelineIntegrationTest,
       EmptyDocumentPublishesGrammarParseDiagnostic) {
  auto document = updateDocument("pipeline-empty.calc", "");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, workspace::DocumentState::Validated);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front()->message, "Expecting 'module'");
  EXPECT_EQ(diagnostics.front()->begin, 0u);
  EXPECT_EQ(diagnostics.front()->end, 0u);
  ASSERT_EQ(document->parseResult.parseDiagnostics.size(), 1u);
  EXPECT_EQ(document->parseResult.parseDiagnostics.front().offset, 0u);
}

TEST_F(DocumentPipelineIntegrationTest,
       IncompleteRecoveredDocumentPublishesParseDiagnostic) {
  auto document = updateDocument(
      "pipeline-incomplete.calc",
      "module mathModule\n"
      "def ");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, workspace::DocumentState::Validated);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front()->message, "Expecting ID");
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingModuleNameAtEofAnchorsDiagnosticAfterModuleKeyword) {
  auto document = updateDocument(
      "pipeline-missing-module-name-eof.calc",
      "module\n"
      "\n");

  ASSERT_NE(document, nullptr);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front()->message, "Expecting ID");
  EXPECT_EQ(diagnostics.front()->begin, 6u);
  EXPECT_EQ(diagnostics.front()->end, 6u);
}

TEST_F(DocumentPipelineIntegrationTest,
       PartialDefinitionFragmentPublishesGenericParseDiagnostic) {
  auto document = updateDocument(
      "pipeline-partial-def-keyword.calc",
      "module mod\n"
      "de\n");

  ASSERT_NE(document, nullptr);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_EQ(diagnostics.size(), 2u);
  EXPECT_TRUE(std::ranges::all_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->source == "parse";
  }));
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->begin >= 11u && diagnostic->begin <= 13u;
  }));
}

TEST_F(DocumentPipelineIntegrationTest,
       TrailingStatementFragmentPublishesGrammarParseDiagnostic) {
  auto document = updateDocument(
      "pipeline-trailing-expression.calc",
      "module name\n"
      "2   *");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, workspace::DocumentState::Validated);

  const auto diagnostics = parseDiagnostics(*document);
  const auto parseDump = dumpDiagnostics(diagnostics);
  ASSERT_FALSE(diagnostics.empty()) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->message.find("NUMBER") != std::string::npos ||
           diagnostic->message.find("ID") != std::string::npos;
  })) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->begin >= 11u && diagnostic->begin <= 17u;
  })) << parseDump;
  ASSERT_FALSE(document->parseResult.parseDiagnostics.empty());
  EXPECT_EQ(document->parseResult.parseDiagnostics.back().offset, 17u);
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingModuleNameRecoveryStillBuildsValidatedDocument) {
  auto document = updateDocument(
      "pipeline-missing-module-name.calc",
      "module \n"
      "\n"
      "def a :4*6;\n");

  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(document->parseSucceeded());
  ASSERT_TRUE(document->parseRecovered());
  ASSERT_FALSE(document->parseResult.parseDiagnostics.empty());
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_GT(parseDiagnosticCount(*document), 0u);
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingDefinitionExpressionBeforeSemicolonLeavesRecoveredHole) {
  auto document = updateDocument(
      "pipeline-missing-definition-expr.calc",
      "module demo\n"
      "\n"
      "def a :;\n");

  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(document->parseSucceeded());
  ASSERT_TRUE(document->parseRecovered());

  auto *module =
      dynamic_cast<arithmetics::ast::Module *>(document->parseResult.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 1u);

  auto *definition =
      dynamic_cast<arithmetics::ast::Definition *>(module->statements.front().get());
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->name, "a");
  EXPECT_EQ(definition->expr.get(), nullptr);
  EXPECT_TRUE(test::has_diagnostic_message(*document, "Expression"));
  EXPECT_FALSE(test::has_diagnostic_message(*document, "Expecting `(`"));
  EXPECT_FALSE(
      test::has_diagnostic_message(*document, "token of type 'DeclaredParameter'"));
  EXPECT_FALSE(test::has_diagnostic_message(*document, "Expecting `)`"));
  EXPECT_FALSE(
      test::has_diagnostic_message(*document, "Unknown reference in function call."));
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingSemicolonAtEndOfEvaluationPublishesTerminatorInDiagnostic) {
  auto document = updateDocument(
      "pipeline-missing-semicolon-eof.calc",
      "module name\n"
      "\n"
      "3 + 5");

  ASSERT_NE(document, nullptr);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front()->message, "Expecting ;");
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingSemicolonBeforeHiddenGapAnchorsAfterPreviousVisibleToken) {
  auto document = updateDocument(
      "pipeline-missing-semicolon-before-gap.calc",
      "module mod\n"
      "def a : 0\n"
      "\n"
      "def ID : 0 ;\n");

  ASSERT_NE(document, nullptr);

  const auto diagnostics = parseDiagnostics(*document);
  ASSERT_FALSE(diagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->message == "Expecting ;" ||
           diagnostic->message.find("Unexpected token") != std::string::npos;
  }));
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->begin >= 11u && diagnostic->begin <= 20u;
  }));
}

TEST_F(DocumentPipelineIntegrationTest,
       MissingSemicolonBeforeNextStatementPublishesGenericParseDiagnostic) {
  auto document = updateDocument(
      "pipeline-missing-separator.calc",
      "module name\n"
      "\n"
      "2  \n"
      "\n"
      "222222 * 7;\n");

  ASSERT_NE(document, nullptr);
  ASSERT_TRUE(document->parseSucceeded());
  ASSERT_TRUE(document->parseRecovered());

  const auto diagnostics = parseDiagnostics(*document);
  const auto parseDump = dumpDiagnostics(diagnostics);
  ASSERT_FALSE(diagnostics.empty()) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->message == "Expecting ;" ||
           diagnostic->message.find("Unexpected token") != std::string::npos;
  })) << parseDump;
  EXPECT_TRUE(std::ranges::any_of(diagnostics, [](const auto *diagnostic) {
    return diagnostic->begin == 13u || diagnostic->begin == 14u;
  })) << parseDump;
}

TEST_F(DocumentPipelineIntegrationTest,
       RecoveredDocumentStillPublishesLateValidationDiagnostic) {
  auto document = updateDocument(
      "pipeline-recovered-late-validation.calc",
      "module calc\n"
      "2*4+7-7/1*9-711*69*22;v\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22/0;\n");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->state, workspace::DocumentState::Validated);
  ASSERT_TRUE(document->parseSucceeded());
  ASSERT_TRUE(document->parseRecovered());
  EXPECT_GT(parseDiagnosticCount(*document), 0u);
  EXPECT_TRUE(test::has_diagnostic_message(*document, "Division by zero"));
  EXPECT_FALSE(
      test::has_diagnostic_message(*document, "Unknown reference in function call."));
}

} // namespace
} // namespace pegium::integration
