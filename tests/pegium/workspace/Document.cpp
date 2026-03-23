#include <gtest/gtest.h>

#include <span>
#include <stdexcept>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/ParseSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/syntax-tree/RootCstNode.hpp>
#include <pegium/core/workspace/Document.hpp>

namespace pegium::workspace {
namespace {

using namespace pegium::parser;

struct IndexedNode final : AstNode {
  string name;
};

struct IndexedRoot final : AstNode {
  vector<pointer<IndexedNode>> nodes;
};

class DocumentIndexParser final : public PegiumParser {
public:
  using PegiumParser::parse;

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
  Rule<IndexedNode> NodeRule{"IndexedNode",
                             "node"_kw + assign<&IndexedNode::name>(ID)};
  Rule<IndexedRoot> RootRule{"IndexedRoot", some(append<&IndexedRoot::nodes>(NodeRule))};
#pragma clang diagnostic pop
};

class AttachingDocumentFactory final : public DocumentFactory {
public:
  using DocumentFactory::attachTextDocument;

  [[nodiscard]] std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<TextDocument>,
      const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<Document>
  fromString(std::string, std::string,
             const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<Document>
  fromUri(std::string_view,
          const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  Document &update(Document &document,
                   const utils::CancellationToken & = {}) const override {
    return document;
  }
};

TEST(DocumentTest, AttachTextDocumentMirrorsSnapshotMetadata) {
  Document document(test::make_text_document("file:///document.test", "test",
                                             "hello", 7));

  EXPECT_EQ(document.uri, "file:///document.test");
  EXPECT_EQ(document.textDocument().getText(), "hello");
  EXPECT_EQ(document.textDocument().languageId(), "test");
  EXPECT_EQ(document.textDocument().version(), 7);
}

TEST(DocumentTest, AttachTextDocumentRebindsSnapshotWithoutResettingState) {
  AttachingDocumentFactory factory;
  Document document(
      test::make_text_document("file:///document.test", "test", "before"));
  document.state = DocumentState::Validated;
  document.parseResult.fullMatch = true;
  document.localSymbols.emplace(nullptr, AstNodeDescription{});
  document.references.clear();
  document.diagnostics.push_back({});

  auto updated = std::make_shared<TextDocument>(document.textDocument());
  const TextDocumentContentChangeEvent change{.text = "after"};
  (void)TextDocument::update(*updated, std::span(&change, std::size_t{1}),
                             updated->version() + 1);
  factory.attachTextDocument(document, std::move(updated));

  EXPECT_EQ(document.state, DocumentState::Validated);
  EXPECT_TRUE(document.parseResult.fullMatch);
  EXPECT_FALSE(document.localSymbols.empty());
  EXPECT_FALSE(document.diagnostics.empty());
  EXPECT_EQ(document.textDocument().getText(), "after");
}

TEST(DocumentTest, FindsAstNodesBySymbolId) {
  Document document(test::make_text_document("file:///document.test", "test",
                                             "node alpha\nnode beta\n"));

  DocumentIndexParser parser;
  pegium::test::apply_parse_result(
      document, parser.parse(document.textDocument().getText()));

  auto *root = dynamic_cast<IndexedRoot *>(document.parseResult.value.get());
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->nodes.size(), 2u);

  const auto firstSymbolId = document.makeSymbolId(*root->nodes[0]);
  const auto secondSymbolId = document.makeSymbolId(*root->nodes[1]);
  const auto unknownSymbolId = secondSymbolId + 1;

  EXPECT_EQ(document.findAstNode(firstSymbolId), root->nodes[0].get());
  EXPECT_EQ(document.findAstNode(secondSymbolId), root->nodes[1].get());
  EXPECT_EQ(document.findAstNode(unknownSymbolId), nullptr);
}

TEST(DocumentTest, ExistingCstTextRemainsValidAfterReplacingEquivalentSnapshot) {
  AttachingDocumentFactory factory;
  Document document(test::make_text_document("file:///document.test", "test",
                                             "node alpha\nnode beta\n"));

  document.parseResult.cst = std::make_unique<RootCstNode>(
      text::TextSnapshot::copy(document.textDocument().getText()));
  document.parseResult.cst->attachDocument(document);
  ASSERT_NE(document.parseResult.cst, nullptr);
  EXPECT_EQ(document.parseResult.cst->getText(), "node alpha\nnode beta\n");

  auto replacement = std::make_shared<TextDocument>(TextDocument::create(
      document.uri, document.textDocument().languageId(), 5,
      "node alpha\nnode beta\n"));
  factory.attachTextDocument(document, replacement);

  std::vector<std::string> heapNoise(1024, std::string(256, 'x'));
  (void)heapNoise;

  ASSERT_NE(document.parseResult.cst, nullptr);
  EXPECT_EQ(document.parseResult.cst->getText(), "node alpha\nnode beta\n");
}

TEST(DocumentTest,
     AttachTextDocumentKeepsAnalysisStateWhenOnlyLanguageIdChanges) {
  AttachingDocumentFactory factory;
  Document document(
      test::make_text_document("file:///document.test", "test", "hello"));
  document.state = DocumentState::Validated;
  document.parseResult.fullMatch = true;
  document.parseResult.parsedLength = 5;

  auto replacement = std::make_shared<TextDocument>(
      TextDocument::create(document.uri, "other", 1, "hello"));
  factory.attachTextDocument(document, replacement);

  EXPECT_EQ(document.state, DocumentState::Validated);
  EXPECT_TRUE(document.parseResult.fullMatch);
  EXPECT_EQ(document.parseResult.parsedLength, 5u);
  EXPECT_EQ(document.textDocument().languageId(), "other");
}

TEST(DocumentTest, ConversionDiagnosticsDoNotMarkDocumentAsRecovered) {
  Document document(
      test::make_text_document("file:///document.test", "test", "hello"));
  document.parseResult.parseDiagnostics.push_back(
      {.kind = parser::ParseDiagnosticKind::ConversionError});
  EXPECT_FALSE(document.parseRecovered());

  document.parseResult.parseDiagnostics.push_back(
      {.kind = parser::ParseDiagnosticKind::Inserted});
  EXPECT_TRUE(document.parseRecovered());
}

} // namespace
} // namespace pegium::workspace
