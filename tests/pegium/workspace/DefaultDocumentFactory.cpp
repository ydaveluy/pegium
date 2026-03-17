#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/utils/UriUtils.hpp>
#include <pegium/workspace/DefaultDocumentFactory.hpp>

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

  void parse(workspace::Document &document,
             const utils::CancellationToken &cancelToken) const override {
    utils::throw_if_cancelled(cancelToken);
    ++parseCalls;
    seenLinkers.push_back(coreServices.references.linker.get());
    PegiumParser::parse(document, cancelToken);
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

TEST(DefaultDocumentFactoryTest, FromTextDocumentResolvesLanguageIdFromRegistry) {
  auto shared = test::make_shared_core_services();
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"}, {}, std::move(parser))));

  DefaultDocumentFactory factory(*shared);

  auto textDocument = std::make_shared<TextDocument>();
  textDocument->uri = test::make_file_uri("factory-resolve.test");
  textDocument->replaceText("alpha");
  textDocument->setClientVersion(4);

  auto document = factory.fromTextDocument(textDocument);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->languageId, "test");
  EXPECT_EQ(document->text(), "alpha");
  EXPECT_EQ(document->clientVersion(), 4);
  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

TEST(DefaultDocumentFactoryTest, ParsePassesRegisteredLinkerToParserContext) {
  auto shared = test::make_shared_core_services();
  TrackingParser::reset();
  auto services = test::make_core_services<TrackingParser>(*shared, "test",
                                                           {".test"});
  auto *linkerPtr = services->references.linker.get();
  ASSERT_NE(linkerPtr, nullptr);
  ASSERT_TRUE(shared->serviceRegistry->registerServices(std::move(services)));

  DefaultDocumentFactory factory(*shared);
  auto document =
      factory.fromString("alpha", test::make_file_uri("factory-linker.test"),
                         "test", 1);

  ASSERT_NE(document, nullptr);
  ASSERT_EQ(TrackingParser::parseCalls, 1u);
  ASSERT_EQ(TrackingParser::seenLinkers.size(), 1u);
  EXPECT_EQ(TrackingParser::seenLinkers.front(), linkerPtr);
}

TEST(DefaultDocumentFactoryTest, FromUriPrefersOpenedTextDocumentOverFileSystem) {
  auto shared = test::make_shared_core_services();
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"}, {}, std::move(parser))));

  auto fileSystem = std::make_shared<test::FakeFileSystemProvider>();
  fileSystem->files["/tmp/pegium-tests/factory-opened.test"] = "from-disk";
  shared->workspace.fileSystemProvider = fileSystem;

  const auto uri = test::make_file_uri("factory-opened.test");
  ASSERT_NE(shared->workspace.textDocuments->open(uri, "test", "from-editor", 3),
            nullptr);

  DefaultDocumentFactory factory(*shared);
  auto document = factory.fromUri(uri);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->text(), "from-editor");
  EXPECT_EQ(document->clientVersion(), 3);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 1u);
  EXPECT_EQ(parserPtr->parsedTexts.front(), "from-editor");
}

TEST(DefaultDocumentFactoryTest,
     UpdateUsesLatestTextDocumentSnapshotAndReparsesChangedDocument) {
  auto shared = test::make_shared_core_services();
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"}, {}, std::move(parser))));

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-update.test");
  auto document = factory.fromString("before", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  EXPECT_EQ(parserPtr->parseCalls, 1u);

  ASSERT_NE(shared->workspace.textDocuments->open(uri, "test", "after", 2),
            nullptr);
  factory.update(*document);

  EXPECT_EQ(document->text(), "after");
  EXPECT_EQ(document->clientVersion(), 2);
  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(parserPtr->parseCalls, 2u);
  ASSERT_EQ(parserPtr->parsedTexts.size(), 2u);
  EXPECT_EQ(parserPtr->parsedTexts.back(), "after");
}

TEST(DefaultDocumentFactoryTest,
     FromUriNormalizesUriBeforeLookingUpOpenedTextDocuments) {
  auto shared = test::make_shared_core_services();
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"}, {}, std::move(parser))));

  const auto normalized =
      utils::path_to_file_uri("/tmp/pegium-tests/factory-uri-normalized.test");
  const auto unnormalized =
      std::string("file:///tmp/pegium-tests/folder/../factory-uri-normalized.test");

  ASSERT_NE(shared->workspace.textDocuments->open(normalized, "test",
                                                  "from-editor", 3),
            nullptr);

  DefaultDocumentFactory factory(*shared);
  auto document = factory.fromUri(unnormalized);

  ASSERT_NE(document, nullptr);
  EXPECT_EQ(document->uri, normalized);
  EXPECT_EQ(document->text(), "from-editor");
  EXPECT_EQ(document->clientVersion(), 3);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

TEST(DefaultDocumentFactoryTest, UpdateDoesNotReparseWhenTextDidNotChange) {
  auto shared = test::make_shared_core_services();
  auto parser = std::make_unique<test::FakeParser>();
  auto *parserPtr = parser.get();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services(*shared, "test", {".test"}, {}, std::move(parser))));

  DefaultDocumentFactory factory(*shared);
  const auto uri = test::make_file_uri("factory-unchanged.test");
  auto document = factory.fromString("same", uri, "test", 1);
  ASSERT_NE(document, nullptr);
  ASSERT_EQ(parserPtr->parseCalls, 1u);

  document->state = DocumentState::Changed;
  ASSERT_NE(shared->workspace.textDocuments->open(uri, "test", "same", 2),
            nullptr);
  factory.update(*document);

  EXPECT_EQ(document->state, DocumentState::Parsed);
  EXPECT_EQ(document->text(), "same");
  EXPECT_EQ(document->clientVersion(), 2);
  EXPECT_EQ(parserPtr->parseCalls, 1u);
}

} // namespace
} // namespace pegium::workspace
