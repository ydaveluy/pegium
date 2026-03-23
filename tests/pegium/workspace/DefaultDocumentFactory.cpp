#include <gtest/gtest.h>

#include <span>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/utils/UriUtils.hpp>
#include <pegium/core/workspace/DefaultDocumentFactory.hpp>

namespace pegium::workspace {
namespace {

using namespace pegium::parser;

struct TrackingNode final : AstNode {
  string value;
};

class TrackingParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

  static void reset() {
    parseCalls = 0;
    seenLinkers.clear();
  }

  static inline std::size_t parseCalls = 0;
  static inline std::vector<const references::Linker *> seenLinkers;

  [[nodiscard]] ParseResult
  parse(text::TextSnapshot text,
        const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    ++parseCalls;
    seenLinkers.push_back(services.references.linker.get());
    return PegiumParser::parse(std::move(text), cancelToken);
  }

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return RootRule;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = SkipperBuilder().ignore(WS).build();
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<TrackingNode> RootRule{"Root", assign<&TrackingNode::value>(ID)};
#pragma clang diagnostic pop
};

std::shared_ptr<TextDocument>
make_text_document(std::string uri, std::string languageId, std::string_view text,
                   std::optional<std::int64_t> version = std::nullopt) {
  return std::make_shared<TextDocument>(TextDocument::create(
      std::move(uri), std::move(languageId), version.value_or(0),
      std::string(text)));
}

TEST(DefaultDocumentFactoryTest, FromTextDocumentResolvesLanguageIdFromRegistry) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);

  auto textDocument = std::make_shared<TextDocument>(TextDocument::create(
      test::make_file_uri("factory-resolve.test"), "", 4, "alpha"));

  auto document = factory.fromTextDocument(textDocument);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->textDocument().getText(), "alpha");
  EXPECT_EQ(document->textDocument().languageId(), "test");
  EXPECT_EQ(document->textDocument().version(), 4);
  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

TEST(DefaultDocumentFactoryTest,
     FromTextDocumentIgnoresUnpublishedLanguageIdHint) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto testParser = std::make_unique<test::FakeParser>();
  auto *testParserPtr = testParser.get();
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(testParser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto reqParser = std::make_unique<test::FakeParser>();
  auto *reqParserPtr = reqParser.get();
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "req", {".req"}, {}, std::move(reqParser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  auto document = factory.fromTextDocument(make_text_document(
      test::make_file_uri("factory-preferred.test"), "req", "alpha", 4));

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->textDocument().languageId(), "test");
  EXPECT_EQ(document->textDocument().getText(), "alpha");
  EXPECT_EQ(document->textDocument().version(), 4);
  EXPECT_EQ(testParserPtr->parseCalls, 1u);
  EXPECT_EQ(reqParserPtr->parseCalls, 0u);
}

TEST(DefaultDocumentFactoryTest, ParsePassesRegisteredLinkerToParserContext) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  TrackingParser::reset();
  auto services = test::make_uninstalled_core_services<TrackingParser>(*shared, "test",
                                                           {".test"});
  pegium::services::installDefaultCoreServices(*services);
  auto *linkerPtr = services->references.linker.get();
  ASSERT_NE(linkerPtr, nullptr);
  shared->serviceRegistry->registerServices(std::move(services));

  DefaultDocumentFactory factory(*shared);
  auto document =
      factory.fromString("alpha", test::make_file_uri("factory-linker.test"));

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(TrackingParser::parseCalls, 1u);
  ASSERT_EQ(TrackingParser::seenLinkers.size(), 1u);
  EXPECT_EQ(TrackingParser::seenLinkers.front(), linkerPtr);
}

TEST(DefaultDocumentFactoryTest, FromUriPrefersOpenedTextDocumentOverFileSystem) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  fileSystem->files["/tmp/pegium-tests/factory-opened.test"] = "from-disk";
  shared->workspace.fileSystemProvider = fileSystem;

  const auto uri = test::make_file_uri("factory-opened.test");
  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(test::set_text_document(*documents, uri, "test", "from-editor", 3),
            nullptr);

  DefaultDocumentFactory factory(*shared);
  auto document = factory.fromUri(uri);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->textDocument().getText(), "from-editor");
  EXPECT_EQ(document->textDocument().version(), 3);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 1u);
  EXPECT_EQ(parserPtr->parsedTexts.front(), "from-editor");
}

TEST(DefaultDocumentFactoryTest, FromUriPropagatesFileSystemErrors) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }
  shared->workspace.fileSystemProvider =
      std::make_shared<EmptyFileSystemProvider>();

  DefaultDocumentFactory factory(*shared);

  EXPECT_THROW((void)factory.fromUri(test::make_file_uri("factory-missing.test")),
               std::runtime_error);
}

TEST(DefaultDocumentFactoryTest,
     UpdateUsesLatestTextDocumentSnapshotAndReparsesChangedDocument) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-update.test");
  auto document =
      factory.fromTextDocument(make_text_document(uri, "test", "before", 1));
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(parserPtr->parseCalls, 1u);

  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(test::set_text_document(*documents, uri, "test", "after", 2),
            nullptr);
  factory.update(*document);

  EXPECT_EQ(document->textDocument().getText(), "after");
  EXPECT_EQ(document->textDocument().version(), 2);
  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(parserPtr->parseCalls, 2u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 2u);
  EXPECT_EQ(parserPtr->parsedTexts.back(), "after");
}

TEST(DefaultDocumentFactoryTest,
     UpdateParsesChangedDocumentEvenWhenCurrentTextAlreadyMatches) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices =
        test::make_uninstalled_core_services(*shared, "test", {".test"}, {},
                                             std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  auto document = std::make_shared<Document>(
      make_text_document(test::make_file_uri("factory-changed.test"), "test",
                         "same", 1));
  ASSERT_NE(
      test::set_text_document(*documents, document->uri, "test", "same", 1),
      nullptr);

  EXPECT_EQ(document->state, DocumentState::Changed);
  EXPECT_EQ(parserPtr->parseCalls, 0u);

  factory.update(*document);

  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(document->textDocument().getText(), "same");
  EXPECT_EQ(parserPtr->parseCalls, 1u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 1u);
  EXPECT_EQ(parserPtr->parsedTexts.front(), "same");
}

TEST(DefaultDocumentFactoryTest,
     UpdateReparsesWhenOpenedTextDocumentMutatesInPlace) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices =
        test::make_uninstalled_core_services(*shared, "test", {".test"}, {},
                                             std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-update-in-place.test");
  auto document =
      factory.fromTextDocument(make_text_document(uri, "test", "before", 1));
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(parserPtr->parseCalls, 1u);

  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  auto opened = test::set_text_document(*documents, uri, "test", "before", 1);
  ASSERT_NE(opened, nullptr);
  const auto *openedPtr = opened.get();

  const TextDocumentContentChangeEvent change{.text = "after"};
  (void)TextDocument::update(*opened, std::span(&change, std::size_t{1}), 2);
  factory.update(*document);

  EXPECT_EQ(documents->get(uri).get(), openedPtr);
  EXPECT_EQ(document->textDocument().getText(), "after");
  EXPECT_EQ(document->textDocument().version(), 2);
  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(parserPtr->parseCalls, 2u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 2u);
  EXPECT_EQ(parserPtr->parsedTexts.back(), "after");
}

TEST(DefaultDocumentFactoryTest,
     FromUriNormalizesUriBeforeLookingUpOpenedTextDocuments) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/factory-uri-normalized.test");
  const auto unnormalized =
      std::string("file:///tmp/pegium-tests/folder/../factory-uri-normalized.test");

  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(
      test::set_text_document(*documents, normalized, "test", "from-editor", 3),
      nullptr);

  DefaultDocumentFactory factory(*shared);
  auto document = factory.fromUri(unnormalized);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, normalized);
  EXPECT_EQ(document->textDocument().getText(), "from-editor");
  EXPECT_EQ(document->textDocument().version(), 3);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

TEST(DefaultDocumentFactoryTest, UpdateDoesNotReparseWhenTextDidNotChange) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  {
    auto registeredServices = 
      test::make_uninstalled_core_services(*shared, "test", {".test"}, {}, std::move(parser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-unchanged.test");
  auto document =
      factory.fromTextDocument(make_text_document(uri, "test", "same", 1));
  ASSERT_NE(document, nullptr);
  ASSERT_EQ(parserPtr->parseCalls, 1u);

  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(test::set_text_document(*documents, uri, "test", "same", 2),
            nullptr);
  factory.update(*document);

  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(document->textDocument().getText(), "same");
  EXPECT_EQ(document->textDocument().version(), 2);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

TEST(DefaultDocumentFactoryTest,
     UpdateDoesNotReparseWhenOnlyLanguageIdChanged) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);

  auto testParser = std::make_unique<test::FakeParser>();
  auto *testParserPtr = testParser.get();
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "test", {".test"}, {}, std::move(testParser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto otherParser = std::make_unique<test::FakeParser>();
  auto *otherParserPtr = otherParser.get();
  {
    auto registeredServices = test::make_uninstalled_core_services(
        *shared, "other", {".other"}, {}, std::move(otherParser));
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-language-rebind.test");
  auto document =
      factory.fromTextDocument(make_text_document(uri, "test", "same", 1));
  ASSERT_NE(document, nullptr);
  ASSERT_EQ(testParserPtr->parseCalls, 1u);
  ASSERT_EQ(otherParserPtr->parseCalls, 0u);

  document->parseResult.fullMatch = false;
  document->parseResult.parsedLength = 17;

  auto documents = test::text_documents(*shared);
  ASSERT_NE(documents, nullptr);
  ASSERT_NE(test::set_text_document(*documents, uri, "other", "same", 2),
            nullptr);
  factory.update(*document);

  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(document->textDocument().getText(), "same");
  EXPECT_EQ(document->textDocument().languageId(), "other");
  EXPECT_EQ(document->textDocument().version(), 2);
  EXPECT_EQ(testParserPtr->parseCalls, 1u);
  EXPECT_EQ(otherParserPtr->parseCalls, 0u);
  EXPECT_FALSE(document->parseResult.fullMatch);
  EXPECT_EQ(document->parseResult.parsedLength, 17u);
}

} // namespace
} // namespace pegium::workspace
