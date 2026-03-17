#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/AbstractSemanticTokenProvider.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {
namespace {

using namespace pegium::parser;

struct SemanticEntry : AstNode {
  string name;
};

class SemanticParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return EntryRule;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = SkipperBuilder().ignore(WS).build();

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<SemanticEntry> EntryRule{"Entry",
                                "entry"_kw + assign<&SemanticEntry::name>(ID)};
#pragma clang diagnostic pop
};

class TestSemanticTokenProvider final : public AbstractSemanticTokenProvider {
public:
  explicit TestSemanticTokenProvider(const services::Services &services)
      : AbstractSemanticTokenProvider(services) {}

protected:
  void highlightElement(const AstNode &node,
                        const SemanticTokenAcceptor &acceptor) const override {
    if (dynamic_cast<const SemanticEntry *>(&node) == nullptr) {
      return;
    }
    highlightKeyword(node, "entry", "keyword", acceptor);
    highlightProperty(node, "name", "variable", acceptor, {"declaration"});
  }
};

TEST(AbstractSemanticTokenProviderTest, ExposesLegendAndEncodesTokens) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<SemanticParser>(*shared, "semantic", {".semantic"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("semantic.semantic"), "semantic",
      "entry Foo");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("semantic");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  TestSemanticTokenProvider provider(*services);

  const auto options = provider.semanticTokensOptions();
  EXPECT_FALSE(options.legend.tokenTypes.empty());
  EXPECT_FALSE(options.legend.tokenModifiers.empty());
  ASSERT_TRUE(options.full.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::SemanticTokensOptionsFull>(*options.full));
  EXPECT_TRUE(options.range.has_value());

  ::lsp::SemanticTokensParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));

  const auto tokens =
      provider.semanticHighlight(*document, params, utils::default_cancel_token);
  ASSERT_TRUE(tokens.has_value());
  EXPECT_TRUE(tokens->resultId.has_value());
  EXPECT_EQ(tokens->data.size(), 10u);
}

TEST(AbstractSemanticTokenProviderTest, RangeHighlightFiltersTokens) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<SemanticParser>(*shared, "semantic", {".semantic"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("semantic-range.semantic"), "semantic",
      "entry Foo");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("semantic");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  TestSemanticTokenProvider provider(*services);

  ::lsp::SemanticTokensRangeParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.range.start.line = 0;
  params.range.start.character = 6;
  params.range.end.line = 0;
  params.range.end.character = 9;

  const auto tokens = provider.semanticHighlightRange(
      *document, params, utils::default_cancel_token);
  ASSERT_TRUE(tokens.has_value());
  EXPECT_TRUE(tokens->resultId.has_value());
  EXPECT_EQ(tokens->data.size(), 5u);
}

TEST(AbstractSemanticTokenProviderTest, DeltaFallsBackToFullForUnknownSnapshot) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<SemanticParser>(*shared, "semantic", {".semantic"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("semantic-delta-full.semantic"), "semantic",
      "entry Foo");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("semantic");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  TestSemanticTokenProvider provider(*services);

  ::lsp::SemanticTokensDeltaParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  params.previousResultId = "missing";

  const auto tokens = provider.semanticHighlightDelta(
      *document, params, utils::default_cancel_token);
  ASSERT_TRUE(tokens.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::SemanticTokens>(*tokens));
  const auto &full = std::get<::lsp::SemanticTokens>(*tokens);
  EXPECT_TRUE(full.resultId.has_value());
  EXPECT_EQ(full.data.size(), 10u);
}

TEST(AbstractSemanticTokenProviderTest,
     DeltaReturnsEditsForKnownSnapshotAndClearsCacheOnClose) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<SemanticParser>(*shared, "semantic", {".semantic"})));

  const auto uri = test::make_file_uri("semantic-delta.semantic");
  auto document =
      test::open_and_build_document(*shared, uri, "semantic", "entry Foo");
  ASSERT_NE(document, nullptr);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("semantic");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  TestSemanticTokenProvider provider(*services);

  ::lsp::SemanticTokensParams fullParams{};
  fullParams.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  const auto full =
      provider.semanticHighlight(*document, fullParams, utils::default_cancel_token);
  ASSERT_TRUE(full.has_value());
  ASSERT_TRUE(full->resultId.has_value());

  ::lsp::SemanticTokensDeltaParams deltaParams{};
  deltaParams.textDocument.uri =
      ::lsp::DocumentUri(::lsp::Uri::parse(document->uri));
  deltaParams.previousResultId = *full->resultId;

  const auto delta = provider.semanticHighlightDelta(
      *document, deltaParams, utils::default_cancel_token);
  ASSERT_TRUE(delta.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::SemanticTokensDelta>(*delta));
  const auto &edits = std::get<::lsp::SemanticTokensDelta>(*delta);
  EXPECT_TRUE(edits.resultId.has_value());
  EXPECT_TRUE(edits.edits.empty());

  ASSERT_TRUE(shared->workspace.textDocuments != nullptr);
  EXPECT_TRUE(shared->workspace.textDocuments->close(document->uri));

  const auto afterClose = provider.semanticHighlightDelta(
      *document, deltaParams, utils::default_cancel_token);
  ASSERT_TRUE(afterClose.has_value());
  EXPECT_TRUE(std::holds_alternative<::lsp::SemanticTokens>(*afterClose));
}

} // namespace
} // namespace pegium::lsp
