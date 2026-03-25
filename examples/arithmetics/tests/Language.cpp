#include <gtest/gtest.h>

#include <arithmetics/parser/Parser.hpp>
#include <arithmetics/lsp/Module.hpp>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>
#include <lsp/json/json.h>
#include <lsp/serialization.h>

#include <sstream>

namespace arithmetics::tests {
namespace {

using pegium::as_services;

std::string dump_parse_diagnostics(
    const std::vector<pegium::parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    std::ostringstream current;
    current << diagnostic.kind;
    if (diagnostic.element != nullptr) {
      current << ":" << *diagnostic.element;
    }
    if (!diagnostic.message.empty()) {
      current << ":" << diagnostic.message;
    }
    current << "@" << diagnostic.beginOffset << "-" << diagnostic.endOffset;
    dump += current.str();
  }
  return dump;
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
     RecoveryPrefersInsertedSemicolonBeforeNextDefinition) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "Module basicMath\n"
      "\n"
      "def a: 5\n"
      "def b: 3;\n",
      pegium::test::make_file_uri("missing-semicolon-before-def.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Inserted;
      }));
  EXPECT_FALSE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted;
      }));

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 2u);

  auto *firstDefinition =
      dynamic_cast<ast::Definition *>(module->statements[0].get());
  auto *secondDefinition =
      dynamic_cast<ast::Definition *>(module->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr);
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(firstDefinition->name, "a");
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(ArithmeticsLanguageTest, RecoverySplitsWordBoundaryAfterModuleKeyword) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, "ModulebasicMath\n", pegium::test::make_file_uri("module-gap.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Inserted;
      }));
  EXPECT_FALSE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted;
      }));

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->name, "basicmath");
}

TEST(ArithmeticsLanguageTest, RecoveryRepairsMissingKeywordCodepointForModule) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, "Modle basicMath\n",
      pegium::test::make_file_uri("module-missing-char.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto replaced =
      std::ranges::find_if(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, parsed.parseDiagnostics.end());
  EXPECT_EQ(replaced->offset, 0u);
  EXPECT_NE(replaced->element, nullptr);

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->name, "basicmath");
}

TEST(ArithmeticsLanguageTest, RecoveryRepairsMissingKeywordSuffixForModule) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser, "Mod basicMath\n",
      pegium::test::make_file_uri("module-missing-suffix.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto replaced =
      std::ranges::find_if(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, parsed.parseDiagnostics.end());
  EXPECT_EQ(replaced->offset, 0u);

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  EXPECT_EQ(module->name, "basicmath");
}

TEST(ArithmeticsLanguageTest, RecoveryRepairsMissingCodepointForDefKeyword) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module basicMath\n"
      "\n"
      "de a: 5;\n",
      pegium::test::make_file_uri("def-missing-char.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto replaced =
      std::ranges::find_if(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, parsed.parseDiagnostics.end());

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 1u);
  auto *definition = dynamic_cast<ast::Definition *>(module->statements[0].get());
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->name, "a");
}

TEST(ArithmeticsLanguageTest, RecoveryDeletesUnexpectedTokenAfterOperator) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "Module basicMath\n"
      "\n"
      "def c: 8;\n"
      "\n"
      "2 * +c;\n",
      pegium::test::make_file_uri("unexpected-token-after-operator.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted;
      }));

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);

  const auto results = evaluate_module(*module);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_DOUBLE_EQ(results.front(), 16.0);
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

TEST(ArithmeticsLanguageTest, RecoveryKeepsValidFunctionArgumentPrefixDirectParse) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module basicMath\n"
      "\n"
      "def root(x, y):\n"
      "    x^(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2);\n"
      "\n"
      "Sqrt(81/);\n",
      pegium::test::make_file_uri("recovered-call-prefix-direct.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
    return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted &&
           diag.offset == 85u;
  })) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 3u);

  auto *evaluation = dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(evaluation, nullptr);
  auto *call = dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
  ASSERT_NE(call, nullptr);
  ASSERT_EQ(call->args.size(), 1u);

  auto *argument = dynamic_cast<ast::NumberLiteral *>(call->args.front().get());
  ASSERT_NE(argument, nullptr);
  EXPECT_DOUBLE_EQ(argument->value, 81.0);
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFunctionCallShapeForEmptyArgumentListDirectParse) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module basicMath\n"
      "\n"
      "def root(x, y):\n"
      "    x^(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2);\n"
      "\n"
      "Sqrt(); // 9\n",
      pegium::test::make_file_uri("empty-call-direct.calc"), "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
    if (diag.kind != pegium::parser::ParseDiagnosticKind::Inserted ||
        diag.element == nullptr) {
      return false;
    }
    const auto *element = diag.element;
    if (element->getKind() == pegium::grammar::ElementKind::Assignment) {
      element =
          static_cast<const pegium::grammar::Assignment *>(element)->getElement();
    }
    const auto *expectedRule =
        dynamic_cast<const pegium::grammar::AbstractRule *>(element);
    return expectedRule != nullptr && expectedRule->getName() == "Expression";
  })) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 3u);

  auto *evaluation = dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(evaluation, nullptr);
  auto *call = dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->func.getRefText(), "sqrt");
}

TEST(ArithmeticsLanguageTest,
     RecoveryKeepsFunctionCallShapeForEmptyArgumentListWithoutSemicolonDirectParse) {
  parser::ArithmeticParser parser;
  auto document = pegium::test::parse_document(
      parser,
      "module basicMath\n"
      "\n"
      "def root(x, y):\n"
      "    x^(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2);\n"
      "\n"
      "Sqrt() // 9\n",
      pegium::test::make_file_uri("empty-call-no-semicolon-direct.calc"),
      "arithmetics");

  const auto &parsed = document->parseResult;
  const auto parseDump = dump_parse_diagnostics(parsed.parseDiagnostics);
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [](const auto &diag) {
    if (diag.kind != pegium::parser::ParseDiagnosticKind::Inserted ||
        diag.element == nullptr) {
      return false;
    }
    const auto *element = diag.element;
    if (element->getKind() == pegium::grammar::ElementKind::Assignment) {
      element =
          static_cast<const pegium::grammar::Assignment *>(element)->getElement();
    }
    const auto *expectedRule =
        dynamic_cast<const pegium::grammar::AbstractRule *>(element);
    return expectedRule != nullptr && expectedRule->getName() == "Expression";
  })) << parseDump;

  auto *module = dynamic_cast<ast::Module *>(parsed.value.get());
  ASSERT_NE(module, nullptr);
  ASSERT_EQ(module->statements.size(), 3u);

  auto *evaluation = dynamic_cast<ast::Evaluation *>(module->statements.back().get());
  ASSERT_NE(evaluation, nullptr);
  auto *call = dynamic_cast<ast::FunctionCall *>(evaluation->expression.get());
  ASSERT_NE(call, nullptr);
  EXPECT_EQ(call->func.getRefText(), "sqrt");
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
