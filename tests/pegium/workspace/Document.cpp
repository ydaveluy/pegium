#include <gtest/gtest.h>

#include <pegium/parser/PegiumParser.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <pegium/workspace/Document.hpp>

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

TEST(DocumentTest, SetTextDocumentMirrorsSnapshotMetadata) {
  auto textDocument = std::make_shared<TextDocument>();
  textDocument->uri = "file:///document.test";
  textDocument->languageId = "test";
  textDocument->replaceText("hello");
  textDocument->setClientVersion(7);

  Document document;
  document.setTextDocument(textDocument);

  EXPECT_EQ(document.uri, "file:///document.test");
  EXPECT_EQ(document.languageId, "test");
  EXPECT_EQ(document.text(), "hello");
  EXPECT_EQ(document.revision(), 1u);
  ASSERT_TRUE(document.clientVersion().has_value());
  EXPECT_EQ(*document.clientVersion(), 7);
}

TEST(DocumentTest, ReplaceTextResetsAnalysisState) {
  Document document;
  document.uri = "file:///document.test";
  document.languageId = "test";
  document.replaceText("before");
  document.state = DocumentState::Validated;
  document.parseResult.fullMatch = true;
  document.localSymbols.emplace(nullptr, AstNodeDescription{});
  document.references.clear();
  document.referenceDescriptions.push_back({});
  document.diagnostics.push_back({});

  document.replaceText("after");

  EXPECT_EQ(document.state, DocumentState::Changed);
  EXPECT_FALSE(document.parseResult.fullMatch);
  EXPECT_TRUE(document.localSymbols.empty());
  EXPECT_TRUE(document.referenceDescriptions.empty());
  EXPECT_TRUE(document.diagnostics.empty());
  EXPECT_EQ(document.text(), "after");
}

TEST(DocumentTest, FindsAstNodesBySymbolId) {
  Document document;
  document.uri = "file:///document.test";
  document.languageId = "test";
  document.replaceText("node alpha\nnode beta\n");

  DocumentIndexParser parser;
  parser.parse(document);

  auto *root = dynamic_cast<IndexedRoot *>(document.parseResult.value.get());
  ASSERT_NE(root, nullptr);
  ASSERT_EQ(root->nodes.size(), 2u);

  const auto firstSymbolId = document.makeSymbolId(*root->nodes[0]);
  const auto secondSymbolId = document.makeSymbolId(*root->nodes[1]);

  EXPECT_EQ(document.findAstNode(firstSymbolId), root->nodes[0].get());
  EXPECT_EQ(document.findAstNode(secondSymbolId), root->nodes[1].get());
  EXPECT_EQ(document.findAstNode(InvalidSymbolId), nullptr);
}

TEST(DocumentTest, ExistingCstTextRemainsValidAfterReplacingEquivalentSnapshot) {
  auto initial = std::make_shared<TextDocument>();
  initial->uri = "file:///document.test";
  initial->languageId = "test";
  initial->replaceText("node alpha\nnode beta\n");

  Document document;
  document.setTextDocument(initial);

  document.parseResult.cst = std::make_unique<RootCstNode>(document);
  ASSERT_NE(document.parseResult.cst, nullptr);
  EXPECT_EQ(document.parseResult.cst->getText(), "node alpha\nnode beta\n");

  auto replacement = std::make_shared<TextDocument>();
  replacement->uri = document.uri;
  replacement->languageId = document.languageId;
  replacement->replaceText("node alpha\nnode beta\n");
  replacement->setClientVersion(5);
  document.setTextDocument(replacement);

  std::vector<std::string> heapNoise(1024, std::string(256, 'x'));
  (void)heapNoise;

  ASSERT_NE(document.parseResult.cst, nullptr);
  EXPECT_EQ(document.parseResult.cst->getText(), "node alpha\nnode beta\n");
}

TEST(DocumentTest, ConversionDiagnosticsDoNotMarkDocumentAsRecovered) {
  Document document;
  document.parseResult.parseDiagnostics.push_back(
      {.kind = parser::ParseDiagnosticKind::ConversionError});
  EXPECT_FALSE(document.parseRecovered());

  document.parseResult.parseDiagnostics.push_back(
      {.kind = parser::ParseDiagnosticKind::Inserted});
  EXPECT_TRUE(document.parseRecovered());
}

} // namespace
} // namespace pegium::workspace
