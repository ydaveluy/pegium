#include <gtest/gtest.h>

#include <chrono>
#include <future>
#include <mutex>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/TestRuleParser.hpp>
#include <pegium/core/grammar/Assignment.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/references/DefaultLinker.hpp>
#include <pegium/core/references/ScopeProvider.hpp>
#include <pegium/core/services/Diagnostic.hpp>
#include <pegium/core/validation/DefaultValidationRegistry.hpp>
#include <pegium/core/workspace/DefaultDocumentBuilder.hpp>

namespace pegium::workspace {
namespace {

using namespace std::chrono_literals;

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

parser::ExpectResult
expectation_result(std::initializer_list<parser::ExpectPath> items) {
  parser::ExpectResult expect;
  expect.frontier.assign(items.begin(), items.end());
  expect.reachedAnchor = !expect.frontier.empty();
  return expect;
}

struct RelinkNode : AstNode {
  std::string name;
};

struct RelinkReferrer : AstNode {
  reference<RelinkNode> node;
};

struct RelinkRoot : AstNode {
  std::vector<pointer<RelinkReferrer>> referrers;
};

struct DummyLiteral final : grammar::Literal {
  [[nodiscard]] bool isNullable() const noexcept override { return false; }
  [[nodiscard]] std::string_view getValue() const noexcept override {
    return "x";
  }
  [[nodiscard]] bool isCaseSensitive() const noexcept override { return true; }
};

const grammar::Literal &dummy_literal() noexcept {
  static const DummyLiteral literal;
  return literal;
}

template <typename TargetType>
struct TestReferenceAssignment final : grammar::Assignment {
  explicit TestReferenceAssignment(std::string_view feature) noexcept
      : feature(feature) {}

  [[nodiscard]] constexpr bool isNullable() const noexcept override {
    return false;
  }
  void execute(AstNode *, const CstNodeView &,
               const parser::ValueBuildContext &) const override {}
  [[nodiscard]] grammar::FeatureValue
  getValue(const AstNode *) const override {
    return {};
  }
  [[nodiscard]] const grammar::AbstractElement *getElement() const noexcept override {
    return nullptr;
  }
  [[nodiscard]] std::string_view getFeature() const noexcept override {
    return feature;
  }
  [[nodiscard]] bool isReference() const noexcept override { return true; }
  [[nodiscard]] bool isMultiReference() const noexcept override {
    return false;
  }
  [[nodiscard]] std::type_index getType() const noexcept override {
    return std::type_index(typeid(TargetType));
  }

  std::string_view feature;
};

class EmptyScopeProvider final : public references::ScopeProvider {
public:
  const workspace::AstNodeDescription *
  getScopeEntry(const ReferenceInfo &) const override {
    return nullptr;
  }

  bool visitScopeEntries(
      const ReferenceInfo &,
      utils::function_ref<bool(const workspace::AstNodeDescription &)>) const override {
    return true;
  }
};

class CountingLinker final : public references::DefaultLinker {
public:
  using references::DefaultLinker::DefaultLinker;

  void link(Document &document,
            const utils::CancellationToken &cancelToken) const override {
    ++linkCalls;
    references::DefaultLinker::link(document, cancelToken);
  }

  mutable std::size_t linkCalls = 0;
};

std::unique_ptr<const parser::Parser>
make_unresolved_reference_parser(const references::Linker *&linkerRef) {
  auto parser = std::make_unique<test::FakeParser>();
  parser->callback =
      [&linkerRef](parser::ParseResult &result, std::string_view text) {
    static const TestReferenceAssignment<RelinkNode> assignment("node");
    static const grammar::Literal &literal = dummy_literal();
    assert(linkerRef != nullptr);

    auto root = std::make_unique<RelinkRoot>();
    auto cst = std::make_unique<RootCstNode>(text::TextSnapshot::copy(text));
    CstBuilder builder(*cst);
    builder.leaf(0, static_cast<TextOffset>(text.size()), &literal);
    root->setCstNode(cst->get(0));
    auto referrer = std::make_unique<RelinkReferrer>();
    auto *referrerPtr = referrer.get();
    referrerPtr->setCstNode(cst->get(0));
    referrerPtr->node.initialize(*referrerPtr, "missing", cst->get(0), assignment,
                                 *linkerRef);
    root->referrers.push_back(std::move(referrer));
    referrerPtr->setContainer<RelinkRoot, &RelinkRoot::referrers>(*root, 0);

    result.value = std::move(root);
    result.cst = std::move(cst);
    result.references.push_back(ReferenceHandle::direct(&referrerPtr->node));
  };
  return parser;
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
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test");
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder.test"), "test", "content");

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->state, DocumentState::Validated);
  EXPECT_TRUE(document->diagnostics.empty());
  EXPECT_TRUE(document->parseSucceeded());
}

TEST(DefaultDocumentBuilderTest,
     UpdateRelinksDocumentsWithExistingLinkingErrors) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  const references::Linker *linkerRef = nullptr;
  auto services = test::make_uninstalled_core_services(
      *shared, "relink", {".relink"}, {}, make_unresolved_reference_parser(linkerRef));
  pegium::services::installDefaultCoreServices(*services);
  services->references.scopeProvider = std::make_unique<EmptyScopeProvider>();
  auto linker = std::make_unique<CountingLinker>(*services);
  auto *linkerPtr = linker.get();
  services->references.linker = std::move(linker);
  linkerRef = linkerPtr;
  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("relink.relink"), "relink", "content");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(document->state, DocumentState::Validated);
  ASSERT_EQ(linkerPtr->linkCalls, 1u);
  ASSERT_FALSE(document->references.empty());
  ASSERT_TRUE(document->references.front().getConst()->hasError());

  shared->workspace.documentBuilder->update({}, {});

  EXPECT_EQ(linkerPtr->linkCalls, 2u);
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest,
     ValidationDiagnosticsArePublishedOnValidatedPhase) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  services::Diagnostic diagnostic;
  diagnostic.message = "validation";
  diagnostic.begin = 1;
  diagnostic.end = 3;
  validator->diagnostics.push_back(diagnostic);
  services->validation.documentValidator = std::move(validator);

  shared->serviceRegistry->registerServices(std::move(services));

  std::vector<std::string> validatedUris;
  auto disposable = shared->workspace.documentBuilder->onDocumentPhase(
      DocumentState::Validated,
      [&validatedUris](const std::shared_ptr<Document> &document,
                       utils::CancellationToken) {
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
     ParseInsertedDiagnosticUsesTokenTypeNameForTokenTypes) {
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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
     ParseKeywordDiagnosticUsesLiteralValueForKeywords) {
  using namespace pegium::parser;

  auto parser = std::make_unique<test::FakeParser>();
  parser->fullMatch = false;
  static constexpr auto moduleKeyword = "module"_kw;
  parser::ParseDiagnostic insertedDiagnostic;
  insertedDiagnostic.kind = ParseDiagnosticKind::Inserted;
  insertedDiagnostic.offset = 0;
  insertedDiagnostic.element = std::addressof(moduleKeyword);
  parser->parseDiagnostics.push_back(insertedDiagnostic);
  parser->expectations =
      expectation_result({keyword_expectation(moduleKeyword)});

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-keyword.test"), "test",
      "foo");

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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  parser->parseDiagnostics.push_back(
      {.kind = ParseDiagnosticKind::Replaced,
       .offset = 0,
       .element = std::addressof(moduleKeyword)});

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-parse-replaced-action.test"),
      "test", "modulx");

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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Incomplete,
                                      .offset = 12,
                                      .element = nullptr});
  parser->expectations = expectation_result({rule_expectation(expression)});

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  parser->expectations =
      expectation_result({keyword_expectation(moduleKeyword)});

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("builder-empty-grammar-sequence.test"),
      "test", "");

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
      "Entry", "module"_kw + assign<&EntryNode::moduleName>(moduleId) +
                   "def"_kw + assign<&EntryNode::definitionName>(id)};
  const auto skipper = SkipperBuilder().ignore(whitespace).build();

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {},
        std::make_unique<test::RuleParser>(entry, skipper));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  const auto offset =
      static_cast<TextOffset>(std::string_view{"module aa\ndef "}.size());

  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                                      .offset = offset,
                                      .element = std::addressof(id)});
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                                      .offset = offset,
                                      .element = std::addressof(colon)});
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                                      .offset = offset,
                                      .element = std::addressof(number)});
  parser->parseDiagnostics.push_back({.kind = ParseDiagnosticKind::Inserted,
                                      .offset = offset,
                                      .element = std::addressof(semicolon)});
  parser->expectations = expectation_result({rule_expectation(id)});

  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

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
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test");
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  shared->serviceRegistry->registerServices(std::move(services));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("default-update-validation.test"), "test",
      "content");

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(validatorPtr->validateCalls, 1u);
  ASSERT_EQ(validatorPtr->seenOptions.size(), 1u);
  EXPECT_EQ(validatorPtr->seenOptions.front().categories,
            (std::vector<std::string>{"built-in", "fast"}));
}

TEST(DefaultDocumentBuilderTest, UpdateEmitsChangedAndDeletedDocumentIds) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto existing = std::make_shared<Document>(
      test::make_text_document(test::make_file_uri("deleted.test"), "test",
                               "obsolete"));
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
  auto textDocuments = test::text_documents(*shared);
  ASSERT_NE(textDocuments, nullptr);
  ASSERT_NE(
      test::set_text_document(*textDocuments, changedUri, "test", "content", 1),
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
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto uri = test::make_file_uri("materialized.test");
  auto textDocuments = test::text_documents(*shared);
  ASSERT_NE(textDocuments, nullptr);
  auto textDocument =
      test::set_text_document(*textDocuments, uri, "test", "content", 1);
  ASSERT_NE(textDocument, nullptr);

  const auto documentId =
      shared->workspace.documents->getOrCreateDocumentId(textDocument->uri());
  const std::array<DocumentId, 1> changedDocumentIds{documentId};
  (void)shared->workspace.documentBuilder->update(changedDocumentIds, {});

  auto document = shared->workspace.documents->getDocument(documentId);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, uri);
  EXPECT_EQ(document->textDocument().languageId(), "test");
  EXPECT_EQ(document->textDocument().getText(), "content");
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest, UpdatePrioritizesOpenTextDocuments) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto closedUri = test::make_file_uri("closed-priority.test");
  auto closedDocument =
      shared->workspace.documentFactory->fromString("closed", closedUri);
  ASSERT_NE(closedDocument, nullptr);
  shared->workspace.documents->addDocument(closedDocument);

  const auto openUri = test::make_file_uri("open-priority.test");
  auto openDocument =
      shared->workspace.documentFactory->fromString("open", openUri);
  ASSERT_NE(openDocument, nullptr);
  shared->workspace.documents->addDocument(openDocument);
  auto textDocuments = test::text_documents(*shared);
  ASSERT_NE(textDocuments, nullptr);
  ASSERT_NE(test::set_text_document(*textDocuments, openUri, "test", "open", 2),
            nullptr);

  std::vector<std::string> validatedUris;
  auto disposable = shared->workspace.documentBuilder->onDocumentPhase(
      DocumentState::Validated,
      [&validatedUris](const std::shared_ptr<Document> &document,
                       utils::CancellationToken) {
        validatedUris.push_back(document->uri);
      });

  (void)shared->workspace.documentBuilder->update({}, {});

  ASSERT_EQ(validatedUris.size(), 2u);
  EXPECT_EQ(validatedUris.front(), openUri);
  EXPECT_EQ(validatedUris.back(), closedUri);
}

TEST(DefaultDocumentBuilderTest, BuildDoesNotValidateByDefault) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  shared->serviceRegistry->registerServices(std::move(services));

  const auto uri = test::make_file_uri("build-no-validation.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  bool validatedPhaseReached = false;
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&validatedPhaseReached](std::span<const std::shared_ptr<Document>>,
                               utils::CancellationToken) {
        validatedPhaseReached = true;
      });

  const std::array<std::shared_ptr<Document>, 1> documents{document};
  shared->workspace.documentBuilder->build(documents);

  EXPECT_EQ(validatorPtr->validateCalls, 0u);
  EXPECT_FALSE(validatedPhaseReached);
  EXPECT_EQ(document->state, DocumentState::IndexedReferences);
}

TEST(DefaultDocumentBuilderTest, BuildEmitsUpdateAndAllBuildPhasesInOrder) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto uri = test::make_file_uri("phases.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
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
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Parsed);
      });
  auto indexedContentDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::IndexedContent,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::IndexedContent);
          });
  auto computedScopesDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::ComputedScopes,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::ComputedScopes);
          });
  auto linkedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Linked,
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Linked);
      });
  auto indexedReferencesDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::IndexedReferences,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::IndexedReferences);
          });
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Validated);
      });

  BuildOptions options;
  options.validation = true;
  const std::array<std::shared_ptr<Document>, 1> documents{document};
  shared->workspace.documentBuilder->build(documents, options);
  EXPECT_EQ(updatedDocumentIds, std::vector<DocumentId>{document->id});
  EXPECT_EQ(phases,
            (std::vector<DocumentState>{
                DocumentState::Parsed, DocumentState::IndexedContent,
                DocumentState::ComputedScopes, DocumentState::Linked,
                DocumentState::IndexedReferences, DocumentState::Validated}));
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest,
     BuildSkipsLinkingPhasesWhenEagerLinkingIsDisabled) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto uri = test::make_file_uri("phases-no-link.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  std::vector<DocumentState> phases;
  auto parsedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed,
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Parsed);
      });
  auto indexedContentDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::IndexedContent,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::IndexedContent);
          });
  auto computedScopesDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::ComputedScopes,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::ComputedScopes);
          });
  auto linkedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Linked,
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Linked);
      });
  auto indexedReferencesDisposable =
      shared->workspace.documentBuilder->onBuildPhase(
          DocumentState::IndexedReferences,
          [&phases](std::span<const std::shared_ptr<Document>>,
                    utils::CancellationToken) {
            phases.push_back(DocumentState::IndexedReferences);
          });
  auto validatedDisposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&phases](std::span<const std::shared_ptr<Document>>,
                utils::CancellationToken) {
        phases.push_back(DocumentState::Validated);
      });

  BuildOptions options;
  options.eagerLinking = false;
  options.validation = true;
  const std::array<std::shared_ptr<Document>, 1> documents{document};
  shared->workspace.documentBuilder->build(documents, options);

  EXPECT_EQ(phases, (std::vector<DocumentState>{DocumentState::Parsed,
                                                DocumentState::IndexedContent,
                                                DocumentState::ComputedScopes,
                                                DocumentState::Validated}));
  EXPECT_EQ(document->state, DocumentState::Validated);
}

TEST(DefaultDocumentBuilderTest,
     BuildRevalidatesOnlyMissingValidationCategoriesAndAppendsDiagnostics) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  validator->diagnosticsByCall = {
      services::Diagnostic{.message = "fast-diagnostic"},
      services::Diagnostic{.message = "slow-diagnostic"},
  };
  services->validation.documentValidator = std::move(validator);

  auto registry =
      std::make_unique<validation::DefaultValidationRegistry>(*services);
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "fast");
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "slow");
  services->validation.validationRegistry = std::move(registry);

  shared->serviceRegistry->registerServices(std::move(services));

  const auto uri = test::make_file_uri("partial-validation.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  BuildOptions fastOnly;
  fastOnly.validation = validation::ValidationOptions{.categories = {"fast"}};
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, fastOnly);

  ASSERT_EQ(validatorPtr->validateCalls, 1u);
  ASSERT_EQ(document->diagnostics.size(), 1u);
  EXPECT_EQ(document->diagnostics.front().message, "fast-diagnostic");

  BuildOptions fastAndSlow;
  fastAndSlow.validation =
      validation::ValidationOptions{.categories = {"fast", "slow"}};
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, fastAndSlow);

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
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  auto registry =
      std::make_unique<validation::DefaultValidationRegistry>(*services);
  registry->registerCheck<AstNode>(
      [](const AstNode &, const validation::ValidationAcceptor &) {}, "fast");
  services->validation.validationRegistry = std::move(registry);

  shared->serviceRegistry->registerServices(std::move(services));

  const auto uri = test::make_file_uri("reset-build-state.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  BuildOptions options;
  options.validation = validation::ValidationOptions{.categories = {"fast"}};
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);
  ASSERT_EQ(validatorPtr->validateCalls, 1u);

  shared->workspace.documentBuilder->resetToState(
      *document, DocumentState::ComputedScopes);

  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);
  EXPECT_EQ(validatorPtr->validateCalls, 2u);
}

TEST(DefaultDocumentBuilderTest, ResetToStateChangedClearsDiagnostics) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  validator->diagnostics.push_back(services::Diagnostic{.message = "validation"});
  services->validation.documentValidator = std::move(validator);

  shared->serviceRegistry->registerServices(std::move(services));

  const auto uri = test::make_file_uri("reset-changed.test");
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);
  ASSERT_EQ(document->state, DocumentState::Validated);
  ASSERT_FALSE(document->diagnostics.empty());

  shared->workspace.documentBuilder->resetToState(*document,
                                                  DocumentState::Changed);
  EXPECT_EQ(document->state, DocumentState::Changed);
  EXPECT_TRUE(document->diagnostics.empty());
}

TEST(DefaultDocumentBuilderTest,
     UpdateKeepsIncompleteBuildOptionsUntilPreviousBuildCompletes) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto services = test::make_uninstalled_core_services(*shared, "test", {".test"});
  pegium::services::installDefaultCoreServices(*services);

  auto validator = std::make_unique<test::FakeDocumentValidator>();
  auto *validatorPtr = validator.get();
  services->validation.documentValidator = std::move(validator);

  shared->serviceRegistry->registerServices(std::move(services));

  const auto uri = test::make_file_uri("resume-incomplete-build.test");
  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  fileSystem->files["/tmp/pegium-tests/resume-incomplete-build.test"] =
      "content";
  shared->workspace.fileSystemProvider = fileSystem;
  auto document =
      shared->workspace.documentFactory->fromString("content", uri);
  ASSERT_NE(document, nullptr);
  document->state = DocumentState::Changed;
  shared->workspace.documents->addDocument(document);

  auto &updateOptions = shared->workspace.documentBuilder->updateBuildOptions();
  updateOptions.eagerLinking = false;
  updateOptions.validation = false;

  utils::CancellationTokenSource cancellationSource;
  bool cancelled = false;
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed, [&cancellationSource, &cancelled](
                                 std::span<const std::shared_ptr<Document>>,
                                 utils::CancellationToken) {
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
  updateOptions.validation = true;

  ASSERT_NO_THROW((void)shared->workspace.documentBuilder->update({}, {}));
  EXPECT_EQ(document->state, DocumentState::ComputedScopes);
  EXPECT_EQ(validatorPtr->validateCalls, 0u);

  ASSERT_NO_THROW((void)shared->workspace.documentBuilder->update({}, {}));
  EXPECT_EQ(document->state, DocumentState::Validated);
  EXPECT_EQ(validatorPtr->validateCalls, 1u);
}

TEST(DefaultDocumentBuilderTest,
     WaitUntilThrowsWhenWorkspaceAlreadyValidatedExcludedDocument) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto excludedDocument = shared->workspace.documentFactory->fromString(
      "excluded", test::make_file_uri("wait-excluded.test"));
  auto validatedDocument = shared->workspace.documentFactory->fromString(
      "validated", test::make_file_uri("wait-validated.test"));
  ASSERT_NE(excludedDocument, nullptr);
  ASSERT_NE(validatedDocument, nullptr);
  shared->workspace.documents->addDocument(excludedDocument);
  shared->workspace.documents->addDocument(validatedDocument);

  BuildOptions parseOnlyOptions;
  parseOnlyOptions.validation = false;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{excludedDocument},
      parseOnlyOptions);
  ASSERT_EQ(excludedDocument->state, DocumentState::IndexedReferences);

  BuildOptions validatedOptions;
  validatedOptions.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{validatedDocument},
      validatedOptions);
  ASSERT_EQ(validatedDocument->state, DocumentState::Validated);

  try {
    (void)shared->workspace.documentBuilder->waitUntil(
        DocumentState::Validated, excludedDocument->id);
    FAIL() << "waitUntil should fail for a document excluded from validation.";
  } catch (const std::runtime_error &error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("requiring Validated"), std::string::npos);
    EXPECT_NE(message.find("workspace state is already Validated"),
              std::string::npos);
  }
}

TEST(DefaultDocumentBuilderTest, WaitUntilDocumentReturnsBuiltDocumentId) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-success.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  auto waiter = std::async(std::launch::async, [&shared, document]() {
    return shared->workspace.documentBuilder->waitUntil(
        DocumentState::Validated, document->id);
  });

  BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);

  EXPECT_EQ(waiter.get(), document->id);
}

TEST(DefaultDocumentBuilderTest, WaitUntilWorkspaceUnblocksWhenArmedBeforeBuild) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-workspace-success.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  auto waiter = std::async(std::launch::async, [&shared]() {
    shared->workspace.documentBuilder->waitUntil(DocumentState::Validated);
  });
  EXPECT_EQ(waiter.wait_for(20ms), std::future_status::timeout);

  BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);

  EXPECT_EQ(waiter.wait_for(1s), std::future_status::ready);
  EXPECT_NO_THROW(waiter.get());
}

TEST(DefaultDocumentBuilderTest,
     WaitUntilWorkspaceResolvesAfterBuildPhaseCallbacksForSameState) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-workspace-order.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  std::mutex eventsMutex;
  std::vector<std::string> events;
  std::future<void> waiter;
  auto waiterStartedPromise = std::make_shared<std::promise<void>>();
  auto waiterStarted = waiterStartedPromise->get_future();
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&shared, &events, &eventsMutex, &waiter, &waiterStarted,
       waiterStartedPromise](
          std::span<const std::shared_ptr<Document>>, utils::CancellationToken) {
        {
          std::scoped_lock lock(eventsMutex);
          events.push_back("B");
        }
        waiter = std::async(std::launch::async,
                            [&shared, &events, &eventsMutex,
                             waiterStartedPromise]() {
                              waiterStartedPromise->set_value();
                              shared->workspace.documentBuilder->waitUntil(
                                  DocumentState::Validated);
                              std::scoped_lock lock(eventsMutex);
                              events.push_back("W");
                            });
        EXPECT_EQ(waiterStarted.wait_for(1s), std::future_status::ready);
        EXPECT_EQ(waiter.wait_for(20ms), std::future_status::timeout);
      });

  BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);

  ASSERT_TRUE(waiter.valid());
  EXPECT_EQ(waiter.wait_for(1s), std::future_status::ready);
  EXPECT_NO_THROW(waiter.get());
  EXPECT_EQ(events, (std::vector<std::string>{"B", "W"}));
}

TEST(DefaultDocumentBuilderTest, WaitUntilWorkspaceUnblocksMultipleWaiters) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-workspace-multiple.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  auto waiter1 = std::async(std::launch::async, [&shared]() {
    shared->workspace.documentBuilder->waitUntil(DocumentState::Validated);
  });
  auto waiter2 = std::async(std::launch::async, [&shared]() {
    shared->workspace.documentBuilder->waitUntil(DocumentState::Validated);
  });
  EXPECT_EQ(waiter1.wait_for(20ms), std::future_status::timeout);
  EXPECT_EQ(waiter2.wait_for(20ms), std::future_status::timeout);

  BuildOptions options;
  options.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options);

  EXPECT_EQ(waiter1.wait_for(1s), std::future_status::ready);
  EXPECT_EQ(waiter2.wait_for(1s), std::future_status::ready);
  EXPECT_NO_THROW(waiter1.get());
  EXPECT_NO_THROW(waiter2.get());
}

TEST(DefaultDocumentBuilderTest,
     WaitUntilWorkspaceSurvivesCancelledBuildAndResolvesOnRetry) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-workspace-retry.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  auto waiter = std::async(std::launch::async, [&shared]() {
    shared->workspace.documentBuilder->waitUntil(DocumentState::Validated);
  });

  utils::CancellationTokenSource cancellationSource;
  bool cancelled = false;
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::ComputedScopes,
      [&cancellationSource, &cancelled](std::span<const std::shared_ptr<Document>>,
                                        utils::CancellationToken) {
        if (!cancelled) {
          cancelled = true;
          cancellationSource.request_stop();
        }
      });

  BuildOptions options;
  options.validation = true;
  EXPECT_THROW(shared->workspace.documentBuilder->build(
                   std::array<std::shared_ptr<Document>, 1>{document}, options,
                   cancellationSource.get_token()),
               utils::OperationCancelled);
  EXPECT_EQ(document->state, DocumentState::ComputedScopes);
  EXPECT_EQ(waiter.wait_for(20ms), std::future_status::timeout);

  EXPECT_NO_THROW(shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document}, options));
  EXPECT_EQ(waiter.wait_for(1s), std::future_status::ready);
  EXPECT_NO_THROW(waiter.get());
}

TEST(DefaultDocumentBuilderTest,
     WaitUntilWorkspaceResolvesWhenValidatedPhaseHasNoDocuments) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = shared->workspace.documentFactory->fromString(
      "content", test::make_file_uri("wait-workspace-empty-phase.test"));
  ASSERT_NE(document, nullptr);
  shared->workspace.documents->addDocument(document);

  bool validatedPhaseReached = false;
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Validated,
      [&validatedPhaseReached](std::span<const std::shared_ptr<Document>>,
                               utils::CancellationToken) {
        validatedPhaseReached = true;
      });
  auto waiter = std::async(std::launch::async, [&shared]() {
    shared->workspace.documentBuilder->waitUntil(DocumentState::Validated);
  });
  EXPECT_EQ(waiter.wait_for(20ms), std::future_status::timeout);

  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{document});

  EXPECT_FALSE(validatedPhaseReached);
  EXPECT_EQ(document->state, DocumentState::IndexedReferences);
  EXPECT_EQ(waiter.wait_for(1s), std::future_status::ready);
  EXPECT_NO_THROW(waiter.get());
}

TEST(DefaultDocumentBuilderTest,
     WaitUntilThrowsWhenWorkspaceAlreadyValidatedInterruptedDocument) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto interruptedDocument = shared->workspace.documentFactory->fromString(
      "interrupted", test::make_file_uri("wait-interrupted.test"));
  auto validatedDocument = shared->workspace.documentFactory->fromString(
      "validated", test::make_file_uri("wait-after-interrupted.test"));
  ASSERT_NE(interruptedDocument, nullptr);
  ASSERT_NE(validatedDocument, nullptr);
  shared->workspace.documents->addDocument(interruptedDocument);
  shared->workspace.documents->addDocument(validatedDocument);

  utils::CancellationTokenSource updateCancellation;
  bool cancelled = false;
  auto disposable = shared->workspace.documentBuilder->onBuildPhase(
      DocumentState::Parsed, [&updateCancellation, &cancelled](
                                 std::span<const std::shared_ptr<Document>>,
                                 utils::CancellationToken) {
        if (!cancelled) {
          cancelled = true;
          updateCancellation.request_stop();
        }
      });

  EXPECT_THROW((void)shared->workspace.documentBuilder->update(
                   {}, {}, updateCancellation.get_token()),
               utils::OperationCancelled);
  ASSERT_EQ(interruptedDocument->state, DocumentState::Parsed);

  BuildOptions validatedOptions;
  validatedOptions.validation = true;
  shared->workspace.documentBuilder->build(
      std::array<std::shared_ptr<Document>, 1>{validatedDocument},
      validatedOptions);
  ASSERT_EQ(validatedDocument->state, DocumentState::Validated);

  try {
    (void)shared->workspace.documentBuilder->waitUntil(
        DocumentState::Validated, interruptedDocument->id);
    FAIL() << "waitUntil should fail once the workspace has already advanced.";
  } catch (const std::runtime_error &error) {
    const std::string message = error.what();
    EXPECT_NE(message.find("requiring Validated"), std::string::npos);
    EXPECT_NE(message.find("workspace state is already Validated"),
              std::string::npos);
  }
}

} // namespace
} // namespace pegium::workspace
