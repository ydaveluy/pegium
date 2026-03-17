#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Diagnostic.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/validation/DefaultValidationRegistry.hpp>
#include <pegium/workspace/DefaultDocumentBuilder.hpp>

namespace pegium::workspace {
namespace {

constexpr std::string_view kDefaultCodeActionsKey = "pegiumDefaultCodeActions";

parser::ExpectPath keyword_expectation(const grammar::Literal &literal) {
  return {
      .elements = {std::addressof(literal)},
  };
}

parser::ExpectPath rule_expectation(const grammar::AbstractRule &rule) {
  return {
      .elements = {std::addressof(rule)},
  };
}

parser::ExpectResult expectation_result(
    std::initializer_list<parser::ExpectPath> items) {
  parser::ExpectResult expect;
  expect.frontier.assign(items.begin(), items.end());
  expect.reachedAnchor = !expect.frontier.empty();
  return expect;
}

const services::JsonValue::Array &
default_code_actions(const services::Diagnostic &diagnostic) {
  EXPECT_TRUE(diagnostic.data.has_value());
  EXPECT_TRUE(diagnostic.data->isObject());
  const auto &data = diagnostic.data->object();
  const auto it = data.find(std::string(kDefaultCodeActionsKey));
  EXPECT_NE(it, data.end());
  EXPECT_TRUE(it->second.isArray());
  return it->second.array();
}

TEST(DefaultDocumentBuilderTest, UpdateBuildsChangedDocumentToValidatedState) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test")));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder.test"), "test", "content");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, DocumentState::Validated);
  EXPECT_TRUE(document->diagnostics.empty());
  EXPECT_TRUE(document->parseSucceeded());
}

TEST(DefaultDocumentBuilderTest, ValidationDiagnosticsArePublishedOnValidatedPhase) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test");

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  services::Diagnostic diagnostic;
  diagnostic.message = "validation";
  diagnostic.begin = 1;
  diagnostic.end = 3;
  validator->diagnostics.push_back(diagnostic);
  services->validation.documentValidator = std::move(validator);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  std::vector<std::string> validatedUris;
  auto disposable = shared->workspace.documentBuilder->onDocumentPhase(
      DocumentState::Validated,
      [&validatedUris](const std::shared_ptr<Document> &document) {
        validatedUris.push_back(document->uri);
      });

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("validated.test"), "test", "content");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->state, DocumentState::Validated);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_EQ(document->diagnostics.front().message, "validation");
  EXPECT_EQ(validatedUris, std::vector<std::string>{document->uri});
}

TEST(DefaultDocumentBuilderTest,
     ParseInsertedDiagnosticUsesLangiumStyleForTokenTypes) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  parser::ParseDiagnostic insertedDiagnostic;
  insertedDiagnostic.kind = ParseDiagnosticKind::Inserted;
  insertedDiagnostic.offset = 0;
  insertedDiagnostic.element = std::addressof(id);
  parser->parseDiagnostics.push_back(insertedDiagnostic);
  parser->expectations = expectation_result({rule_expectation(id)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-inserted.test"), "test", "");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting ID");
  ASSERT_TRUE(document->diagnostics.front().code.has_value());
  EXPECT_EQ(std::get<std::string>(*document->diagnostics.front().code),
            "parse.inserted");
  EXPECT_FALSE(document->diagnostics.front().data.has_value());
}

TEST(DefaultDocumentBuilderTest,
     ParseKeywordDiagnosticUsesLangiumStyleForKeywords) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  static constexpr auto moduleKeyword = "module"_kw;
  parser::ParseDiagnostic insertedDiagnostic;
  insertedDiagnostic.kind = ParseDiagnosticKind::Inserted;
  insertedDiagnostic.offset = 0;
  insertedDiagnostic.element = std::addressof(moduleKeyword);
  parser->parseDiagnostics.push_back(insertedDiagnostic);
  parser->expectations = expectation_result({keyword_expectation(moduleKeyword)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-keyword.test"), "test", "foo");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting 'module'");
  const auto &actions = default_code_actions(document->diagnostics.front());
  ASSERT_EQ(actions.size(), 1u);
  const auto &action = actions.front().object();
  EXPECT_EQ(action.at("title").string(), "Insert 'module'");
  EXPECT_EQ(action.at("newText").string(), "module");
}

TEST(DefaultDocumentBuilderTest,
     ParseIncompleteDiagnosticUsesExpectedTokenAtEndOfInput) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  parser::ParseDiagnostic incompleteDiagnostic;
  incompleteDiagnostic.kind = ParseDiagnosticKind::Incomplete;
  incompleteDiagnostic.offset = 0;
  incompleteDiagnostic.element = std::addressof(id);
  parser->parseDiagnostics.push_back(incompleteDiagnostic);
  parser->expectations = expectation_result({rule_expectation(id)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-incomplete-eof.test"), "test",
      "");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting ID");
  EXPECT_FALSE(document->diagnostics.front().data.has_value());
}

TEST(DefaultDocumentBuilderTest,
     ParseDeletedDiagnosticAddsDefaultCodeActionData) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = true;
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Deleted, .offset = 0, .element = nullptr});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-deleted-action.test"), "test",
      "unexpected");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  const auto &actions = default_code_actions(document->diagnostics.front());
  ASSERT_EQ(actions.size(), 1u);
  const auto &action = actions.front().object();
  EXPECT_EQ(action.at("title").string(), "Delete unexpected text");
  EXPECT_EQ(action.at("newText").string(), "");
  EXPECT_EQ(action.at("begin").integer(), 0);
  EXPECT_EQ(action.at("end").integer(), 10);
}

TEST(DefaultDocumentBuilderTest,
     ParseReplacedLiteralDiagnosticAddsDefaultCodeActionData) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = true;
  static constexpr auto moduleKeyword = "module"_kw;
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Replaced,
                                      .offset = 0,
                                      .element = std::addressof(moduleKeyword)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-replaced-action.test"), "test",
      "modulx");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  const auto &actions = default_code_actions(document->diagnostics.front());
  ASSERT_EQ(actions.size(), 1u);
  const auto &action = actions.front().object();
  EXPECT_EQ(action.at("title").string(), "Replace with 'module'");
  EXPECT_EQ(action.at("newText").string(), "module");
  EXPECT_EQ(action.at("begin").integer(), 0);
  EXPECT_EQ(action.at("end").integer(), 6);
}

TEST(DefaultDocumentBuilderTest,
     ParseInsertedRuleDiagnosticDoesNotAddDefaultCodeActionData) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                                      .offset = 0,
                                      .element = std::addressof(id)});
  parser->expectations = expectation_result({rule_expectation(id)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-inserted-rule.test"), "test",
      "");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_FALSE(document->diagnostics.front().data.has_value());
}

TEST(DefaultDocumentBuilderTest,
     ParseRecoveredDiagnosticDoesNotAddDefaultCodeActionData) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = true;
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Recovered, .offset = 0});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-recovered.test"), "test",
      "alpha");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_FALSE(document->diagnostics.front().data.has_value());
}

TEST(DefaultDocumentBuilderTest,
     ParseConversionDiagnosticUsesDedicatedCodeAndSpan) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = true;
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::ConversionError,
       .offset = 4,
       .beginOffset = 4,
       .endOffset = 7,
       .element = nullptr,
       .message = "bad value"});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-conversion.test"), "test",
      "name123");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_EQ(document->diagnostics.front().message, "bad value");
  EXPECT_EQ(document->diagnostics.front().begin, 4u);
  EXPECT_EQ(document->diagnostics.front().end, 7u);
  ASSERT_TRUE(document->diagnostics.front().code.has_value());
  EXPECT_EQ(std::get<std::string>(*document->diagnostics.front().code),
            "parse.conversion");
}

TEST(DefaultDocumentBuilderTest,
     ParseIncompleteDiagnosticWithTrailingInputUsesDirectFrontier) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  DataTypeRule<int> expression{"Expression", some(d)};
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Incomplete, .offset = 12, .element = nullptr});
  parser->expectations = expectation_result({rule_expectation(expression)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-incomplete-trailing.test"),
      "test", "module name\n2   *");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting Expression");
  EXPECT_EQ(document->diagnostics.front().begin, 12u);
  EXPECT_EQ(document->diagnostics.front().end, 12u);
}

TEST(DefaultDocumentBuilderTest,
     EmptyDocumentUsesDirectFrontierInsteadOfEmptyFoundToken) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  static constexpr auto moduleKeyword = "module"_kw;
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = 0,
       .element = std::addressof(moduleKeyword)});
  parser->expectations = expectation_result({keyword_expectation(moduleKeyword)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-empty-grammar-sequence.test"), "test",
      "");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting 'module'");
}

TEST(DefaultDocumentBuilderTest,
     ParseInsertedDiagnosticAfterTriviaAnchorsAfterPreviousVisibleToken) {
  using namespace pegium::parser;

  struct EntryNode : pegium::AstNode {
    string moduleName;
    string definitionName;
  };

  const auto whitespace = some(s);
  TerminalRule<std::string> moduleId{"MODULE_ID", "A-Z"_cr + many(w)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<EntryNode> entry{
      "Entry", "module"_kw + assign<&EntryNode::moduleName>(moduleId) + "def"_kw +
                   assign<&EntryNode::definitionName>(id)};
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {},
      std::make_unique<test::RuleParser>(entry, skipper))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-inserted-gap.test"), "test",
      "module \n\n def a");

  ASSERT_NE(document, nullptr);
  ASSERT_FALSE(document->diagnostics.empty());
  EXPECT_EQ(document->diagnostics.front().message, "Expecting MODULE_ID");
  EXPECT_EQ(document->diagnostics.front().begin, 10u);
  EXPECT_EQ(document->diagnostics.front().end, 10u);
  ASSERT_TRUE(document->parseResult.value != nullptr);
}

TEST(DefaultDocumentBuilderTest,
     ConsecutiveInsertedDiagnosticsAtEndOfInputUseDirectFrontier) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  TerminalRule<int> number{"NUMBER", some(d)};
  static constexpr auto colon = ":"_kw;
  static constexpr auto semicolon = ";"_kw;
  const auto offset = static_cast<TextOffset>(std::string_view{"module aa\ndef "}.size());

  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = offset,
       .element = std::addressof(id)});
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = offset,
       .element = std::addressof(colon)});
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = offset,
       .element = std::addressof(number)});
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Inserted,
       .offset = offset,
       .element = std::addressof(semicolon)});
  parser->expectations = expectation_result({rule_expectation(id)});

  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(test::make_core_services(
      *shared, "test", {".test"}, {}, std::move(parser))));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-inserted-eof-sequence.test"),
      "test", "module aa\ndef ");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_EQ(document->diagnostics.front().message, "Expecting ID");
  ASSERT_TRUE(document->diagnostics.front().code.has_value());
  EXPECT_EQ(std::get<std::string>(*document->diagnostics.front().code),
            "parse.inserted");
}

TEST(DefaultDocumentBuilderTest,
     UpdateUsesBuiltInAndFastValidationCategoriesByDefault) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test");

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("default-update-validation.test"), "test",
      "content");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(validatorPtr->validateCalls, 1u);
  ASSERT_EQ(validatorPtr->seenOptions.size(), 1u);
  EXPECT_TRUE(validatorPtr->seenOptions.front().enabled);
  EXPECT_EQ(validatorPtr->seenOptions.front().categories,
            (std::vector<std::string>{"built-in", "fast"}));
}

TEST(DefaultDocumentBuilderTest, UpdateEmitsChangedAndDeletedDocumentIds) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  auto existing = std::make_shared<Document>();
  existing->uri = test::make_file_uri("deleted.test");
  existing->languageId = "test";
  existing->setText("obsolete");
  shared->workspace.documents->addDocument(existing);

  std::vector<DocumentId> changed;
  std::vector<DocumentId> deleted;
  auto disposable = shared->workspace.documentBuilder->onUpdate(
      [&changed, &deleted](std::span<const DocumentId> changedDocumentIds,
                           std::span<const DocumentId> deletedDocumentIds) {
        changed.assign(changedDocumentIds.begin(), changedDocumentIds.end());
        deleted.assign(deletedDocumentIds.begin(), deletedDocumentIds.end());
      });

  const auto changedUri = test::make_file_uri("changed.test");
  ASSERT_NE(shared->workspace.textDocuments->open(changedUri, "test", "content", 1),
            nullptr);

  const auto changedDocumentId =
      shared->workspace.documents->getOrCreateDocumentId(changedUri);
  const std::array<DocumentId, 1> changedDocumentIds{changedDocumentId};
  const std::array<DocumentId, 1> deletedDocumentIds{existing->id};
  (void)shared->workspace.documentBuilder->update(changedDocumentIds,
                                                  deletedDocumentIds);

  EXPECT_EQ(changed, std::vector<DocumentId>{changedDocumentId});
  EXPECT_EQ(deleted, std::vector<DocumentId>{existing->id});
}

TEST(DefaultDocumentBuilderTest,
     UpdateMaterializesChangedDocumentFromOpenTextDocument) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  const auto uri = test::make_file_uri("materialized.test");
  auto textDocument =
      shared->workspace.textDocuments->open(uri, "test", "content", 1);
  ASSERT_NE(textDocument, nullptr);

  const auto documentId =
      shared->workspace.documents->getOrCreateDocumentId(textDocument->uri);
  const std::array<DocumentId, 1> changedDocumentIds{documentId};
  (void)shared->workspace.documentBuilder->update(changedDocumentIds, {});

  auto document = shared->workspace.documents->getDocument(documentId);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, uri);
  EXPECT_EQ(document->languageId, "test");
  EXPECT_EQ(document->text(), "content");
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest, UpdatePrioritizesOpenTextDocuments) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  const auto closedUri = test::make_file_uri("closed-priority.test");
  auto closedDocument = shared->workspace.documentFactory->fromString(
      "closed", closedUri, "test", 1);
  ASSERT_NE(closedDocument, nullptr);
  shared->workspace.documents->addDocument(closedDocument);

  const auto openUri = test::make_file_uri("open-priority.test");
  auto openDocument = shared->workspace.documentFactory->fromString(
      "open", openUri, "test", 1);
  ASSERT_NE(openDocument, nullptr);
  shared->workspace.documents->addDocument(openDocument);
  ASSERT_NE(shared->workspace.textDocuments->open(openUri, "test", "open", 2),
            nullptr);

  std::vector<std::string> validatedUris;
  auto disposable = shared->workspace.documentBuilder->onDocumentPhase(
      DocumentState::Validated,
      [&validatedUris](const std::shared_ptr<Document> &document) {
        validatedUris.push_back(document->uri);
      });

  (void)shared->workspace.documentBuilder->update({}, {});

  ASSERT_EQ(validatedUris.size(), 2u);
  EXPECT_EQ(validatedUris.front(), openUri);
  EXPECT_EQ(validatedUris.back(), closedUri);
}

TEST(DefaultDocumentBuilderTest, BuildDoesNotValidateByDefault) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test", {".test"});

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  const auto uri = test::make_file_uri("build-no-validation.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  bool validatedPhaseReached = false;
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&validatedPhaseReached](std::span<const std::shared_ptr<Document>>) {
        validatedPhaseReached = true;
      });

  const std::array<std::shared_ptr<Document>, 1> documents{document};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(documents));

  EXPECT_EQ(validatorPtr->validateCalls, 0u);
  EXPECT_FALSE(validatedPhaseReached);
  EXPECT_EQ(document->state, DocumentState::IndexedReferences);
}

TEST(DefaultDocumentBuilderTest, BuildEmitsUpdateAndAllBuildPhasesInOrder) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  const auto uri = test::make_file_uri("phases.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  std::vector<DocumentId> updatedDocumentIds;
  std::vector<DocumentState> phases;
  auto updateDisposable = shared->workspace.documentBuilder->onUpdate(
      [&updatedDocumentIds](std::span<const DocumentId> changedDocumentIds,
                            std::span<const DocumentId>) {
        updatedDocumentIds.assign(changedDocumentIds.begin(),
                                  changedDocumentIds.end());
      });
  auto parsedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Parsed);
      });
  auto indexedContentDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::IndexedContent,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::IndexedContent);
      });
  auto computedScopesDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::ComputedScopes,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::ComputedScopes);
      });
  auto linkedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Linked,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Linked);
      });
  auto indexedReferencesDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::IndexedReferences,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::IndexedReferences);
      });
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Validated);
      });

  BuildOptions options;
  options.validation.enabled = true;
  const std::array<std::shared_ptr<Document>, 1> documents{document};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(documents, options));
  EXPECT_EQ(updatedDocumentIds, std::vector<DocumentId>{document->id});
  EXPECT_EQ(
      phases,
      (std::vector<DocumentState>{DocumentState::Parsed,
                                  DocumentState::IndexedContent,
                                  DocumentState::ComputedScopes,
                                  DocumentState::Linked,
                                  DocumentState::IndexedReferences,
                                  DocumentState::Validated}));
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest, BuildSkipsLinkingPhasesWhenEagerLinkingIsDisabled) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"})));

  const auto uri = test::make_file_uri("phases-no-link.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  std::vector<DocumentState> phases;
  auto parsedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Parsed);
      });
  auto indexedContentDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::IndexedContent,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::IndexedContent);
      });
  auto computedScopesDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::ComputedScopes,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::ComputedScopes);
      });
  auto linkedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Linked,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Linked);
      });
  auto indexedReferencesDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::IndexedReferences,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::IndexedReferences);
      });
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&phases](std::span<const std::shared_ptr<Document>>) {
        phases.push_back(DocumentState::Validated);
      });

  BuildOptions options;
  options.eagerLinking = false;
  options.validation.enabled = true;
  const std::array<std::shared_ptr<Document>, 1> documents{document};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(documents, options));

  EXPECT_EQ(
      phases,
      (std::vector<DocumentState>{DocumentState::Parsed,
                                  DocumentState::IndexedContent,
                                  DocumentState::ComputedScopes,
                                  DocumentState::Validated}));
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest,
     BuildRevalidatesOnlyMissingValidationCategoriesAndAppendsDiagnostics) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test", {".test"});

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  validator->diagnosticsByCall = {
      services::Diagnostic{.message = "fast-diagnostic"},
      services::Diagnostic{.message = "slow-diagnostic"},
  };
  services->validation.documentValidator = std::move(validator);

  auto registry = std::make_unique<validation::DefaultValidationRegistry>();
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "fast");
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "slow");
  services->validation.validationRegistry = std::move(registry);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  const auto uri = test::make_file_uri("partial-validation.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  BuildOptions fastOnly;
  fastOnly.validation.enabled = true;
  fastOnly.validation.categories = {"fast"};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, fastOnly));

  ASSERT_EQ(validatorPtr->validateCalls, 1u);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_EQ(document->diagnostics.front().message, "fast-diagnostic");

  BuildOptions fastAndSlow;
  fastAndSlow.validation.enabled = true;
  fastAndSlow.validation.categories = {"fast", "slow"};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, fastAndSlow));

  ASSERT_EQ(validatorPtr->validateCalls, 2u);
  ASSERT_EQ(validatorPtr->seenOptions.size(), 2u);
  EXPECT_EQ(validatorPtr->seenOptions[0].categories,
            (std::vector<std::string>{"fast"}));
  EXPECT_EQ(validatorPtr->seenOptions[1].categories,
            (std::vector<std::string>{"slow"}));
  ASSERT_EQ(document->diagnostics.size(), 2u);
  EXPECT_EQ(document->diagnostics[0].message, "fast-diagnostic");
  EXPECT_EQ(document->diagnostics[1].message, "slow-diagnostic");
}

TEST(DefaultDocumentBuilderTest,
     ResetToStateClearsBuildStateBelowIndexedReferences) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test", {".test"});

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  auto registry = std::make_unique<validation::DefaultValidationRegistry>();
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "fast");
  services->validation.validationRegistry = std::move(registry);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  const auto uri = test::make_file_uri("reset-build-state.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  BuildOptions options;
  options.validation.enabled = true;
  options.validation.categories = {"fast"};
  ASSERT_TRUE(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options));
  ASSERT_EQ(validatorPtr->validateCalls, 1u);

  shared->workspace.documentBuilder->resetToState(*document,
                                                  DocumentState::ComputedScopes);

  ASSERT_TRUE(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options));
  EXPECT_EQ(validatorPtr->validateCalls, 2u);
}

TEST(DefaultDocumentBuilderTest,
     UpdateKeepsIncompleteBuildOptionsUntilPreviousBuildCompletes) {
  auto shared = test::make_shared_core_services();
  auto services = test::make_core_services(*shared, "test", {".test"});

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  const auto uri = test::make_file_uri("resume-incomplete-build.test");
  auto document = shared->workspace.documentFactory->fromString(
      "content", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  document->state = DocumentState::Changed;
  shared->workspace.documents->addDocument(document);

  auto &updateOptions = shared->workspace.documentBuilder->updateBuildOptions();
  updateOptions.eagerLinking = false;
  updateOptions.validation.enabled = false;

  utils::CancellationTokenSource cancellationSource;
  bool cancelled = false;
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed,
      [&cancellationSource, &cancelled](
          std::span<const std::shared_ptr<Document>>) {
        if (!cancelled) {
          cancelled = true;
          cancellationSource.request_stop();
        }
      });

  EXPECT_THROW((void)shared->workspace.documentBuilder->update(
                   {}, {}, cancellationSource.get_token()),
               utils::OperationCancelled);
  ASSERT_EQ(document->state, DocumentState::Parsed);

  updateOptions.eagerLinking = true;
  updateOptions.validation.enabled = true;

  ASSERT_NO_THROW((void)shared->workspace.documentBuilder->update({}, {}));
  EXPECT_EQ(document->state, DocumentState::ComputedScopes);
  EXPECT_EQ(validatorPtr->validateCalls, 0u);

  ASSERT_NO_THROW((void)shared->workspace.documentBuilder->update({}, {}));
  EXPECT_EQ(document->state, DocumentState::Validated);
  EXPECT_EQ(validatorPtr->validateCalls, 1u);
}

} // namespace
} // namespace pegium::workspace
