#include <gtest/gtest.h>

#include <pegium/LspExpectTestSupport.hpp>
#include <pegium/LspTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>
#include <pegium/workspace/AstDescriptions.hpp>

namespace pegium::lsp {
namespace {

using namespace pegium::parser;

struct SymbolEntry : AstNode {
  string name;
};

struct SymbolModel : AstNode {
  vector<pointer<SymbolEntry>> entries;
};

class SymbolParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return ModelRule;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = SkipperBuilder().ignore(WS).build();

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<SymbolEntry> EntryRule{"Entry",
                              "entry"_kw + assign<&SymbolEntry::name>(ID)};
  Rule<SymbolModel> ModelRule{
      "Model", some(append<&SymbolModel::entries>(EntryRule))};
#pragma clang diagnostic pop
};

TEST(DefaultDocumentSymbolProviderTest, ReturnsEmptyWithoutAstOrNameProvider) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "test", {".test"})));

  workspace::Document document;
  document.id = 1;
  document.uri = test::make_file_uri("symbols.test");
  document.languageId = "test";
  document.setText("alpha beta");

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("test");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->lsp.documentSymbolProvider, nullptr);

  const auto symbols = services->lsp.documentSymbolProvider->getSymbols(
      document, ::lsp::DocumentSymbolParams{}, utils::default_cancel_token);
  EXPECT_TRUE(symbols.empty());
}

TEST(DefaultDocumentSymbolProviderTest, UsesAstNamesWhenAvailable) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services<SymbolParser>(*shared, "symbols", {".symbols"})));

  auto document = pegium::test::expectSymbols(
      *shared, "symbols",
      pegium::test::ExpectedSymbols{
          .text = "entry Alpha\nentry Beta\n",
          .expectedSymbols = {"Alpha", "Beta"},
      });
  ASSERT_NE(document, nullptr);
}

} // namespace
} // namespace pegium::lsp
