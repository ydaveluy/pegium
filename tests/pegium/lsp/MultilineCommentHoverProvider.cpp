#include <gtest/gtest.h>

#include <lsp/json/json.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>
#include <pegium/core/workspace/AstDescriptions.hpp>

namespace pegium {
namespace {

using pegium::as_services;
using namespace pegium::parser;

struct HoverEntry : AstNode {
  string name;
};

struct HoverModel : AstNode {
  string name;
  vector<pointer<HoverEntry>> entries;
};

class HoverParser final : public PegiumParser {
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
  Skipper skipper =
      SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();

  Terminal<> ML_COMMENT{"ML_COMMENT", "/**"_kw <=> "*/"_kw};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<HoverEntry> EntryRule{"Entry",
                             "entry"_kw + assign<&HoverEntry::name>(ID)};
  Rule<HoverModel> ModelRule{
      "Model",
      option("module"_kw + assign<&HoverModel::name>(ID)) +
          some(append<&HoverModel::entries>(EntryRule))};
#pragma clang diagnostic pop
};

TEST(MultilineCommentHoverProviderTest, ReturnsNoHoverWithoutDocumentation) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices =
        test::make_uninstalled_services<HoverParser>(*shared, "test",
                                                     {".test"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("hover.test"), "test", "entry Value\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(document->uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);

  ::lsp::HoverParams params{};
  params.position.line = 0;
  params.position.character = 7;

  const auto hover =
      services->lsp.hoverProvider->getHoverContent(*document, params,
                                                   utils::default_cancel_token);
  EXPECT_FALSE(hover.has_value());
}

TEST(MultilineCommentHoverProviderTest, UsesDocumentationWhenAvailable) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services<HoverParser>(*shared, "docs", {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("hover.docs"), "docs",
      "/**\n"
      " * Hover docs.\n"
      " */\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(document->uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);

  ::lsp::HoverParams params{};
  params.position.line = 3;
  params.position.character = 7;

  const auto hover = services->lsp.hoverProvider->getHoverContent(
      *document, params, utils::default_cancel_token);
  ASSERT_TRUE(hover.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::MarkupContent>(hover->contents));
  const auto &content = std::get<::lsp::MarkupContent>(hover->contents);
  EXPECT_NE(content.value.find("Hover docs."), std::string::npos);
}

TEST(MultilineCommentHoverProviderTest, UsesDocumentationWhenHoveringNamedRootNode) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services<HoverParser>(*shared, "docs", {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("root-hover.docs"), "docs",
      "/**\n"
      " * Root docs.\n"
      " */\n"
      "module Demo\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);

  const auto *coreServices = &shared->serviceRegistry->getServices(document->uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);

  ::lsp::HoverParams params{};
  params.position.line = 3;
  params.position.character = 8;

  const auto hover = services->lsp.hoverProvider->getHoverContent(
      *document, params, utils::default_cancel_token);
  ASSERT_TRUE(hover.has_value());
  ASSERT_TRUE(std::holds_alternative<::lsp::MarkupContent>(hover->contents));
  const auto &content = std::get<::lsp::MarkupContent>(hover->contents);
  EXPECT_NE(content.value.find("Root docs."), std::string::npos);
}

TEST(MultilineCommentHoverProviderTest, HoverMarkupWithControlCharactersSerializes) {
  ::lsp::Hover hover{};
  ::lsp::MarkupContent markup{};
  markup.kind = ::lsp::MarkupKind::Markdown;
  markup.value = std::string{"before\vafter\0tail", 17};
  hover.contents = std::move(markup);

  ::lsp::TextDocument_HoverResult result{};
  result = std::move(hover);

  const auto json = ::lsp::json::stringify(::lsp::toJson(std::move(result)));
  EXPECT_NO_THROW((void)::lsp::json::parse(json));
}

} // namespace
} // namespace pegium
