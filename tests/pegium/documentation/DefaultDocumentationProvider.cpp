#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/documentation/DefaultDocumentationProvider.hpp>
#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

namespace pegium::documentation {
namespace {

using namespace pegium::parser;

struct DocumentationEntry : AstNode {
  string name;
};

struct DocumentationModel : AstNode {
  string name;
  vector<pointer<DocumentationEntry>> entries;
};

class DocumentationParser final : public PegiumParser {
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

  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<DocumentationEntry> EntryRule{"Entry",
                                     "entry"_kw + assign<&DocumentationEntry::name>(ID)};

  Rule<DocumentationModel> ModelRule{
      "Model",
      option("module"_kw + assign<&DocumentationModel::name>(ID)) +
          some(append<&DocumentationModel::entries>(EntryRule))};
#pragma clang diagnostic pop
};

class HookedDocumentationProvider final : public DefaultDocumentationProvider {
public:
  explicit HookedDocumentationProvider(const services::CoreServices &services)
      : DefaultDocumentationProvider(services) {}

protected:
  [[nodiscard]] std::optional<std::string>
  documentationLinkRenderer(const AstNode &node, std::string_view name,
                            std::string_view display) const override {
    (void)node;
    return "<" + std::string(name) + "|" + std::string(display) + ">";
  }

  [[nodiscard]] std::optional<std::string>
  documentationTagRenderer(const AstNode &node,
                           std::string_view tag) const override {
    (void)node;
    return "**" + std::string(tag) + "**";
  }
};

std::shared_ptr<workspace::Document>
parse_docs_document(services::SharedCoreServices &shared, std::string text) {
  return test::open_and_build_document(
      shared, test::make_file_uri("documentation.docs"), "docs",
      std::move(text));
}

TEST(DefaultDocumentationProviderTest,
     ReturnsNulloptForNodeWithoutLeadingComment) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = parse_docs_document(*shared, "entry Plain\n");
  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.documentation.documentationProvider, nullptr);

  EXPECT_EQ(services.documentation.documentationProvider->getDocumentation(
                *model->entries.front()),
            std::nullopt);
}

TEST(DefaultDocumentationProviderTest,
     RendersJSDocMarkdownWithNormalizedLinksAndTags) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = parse_docs_document(
      *shared,
      "/**\n"
      " * Adds numbers.\n"
      " * {@link Value|The Value}\n"
      " * @param p first value\n"
      " */\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.documentation.documentationProvider, nullptr);

  const auto documentation =
      services.documentation.documentationProvider->getDocumentation(
          *model->entries.front());
  ASSERT_TRUE(documentation.has_value());
  EXPECT_NE(documentation->find("Adds numbers."), std::string::npos);
  EXPECT_NE(documentation->find(
                "[The Value](file:///tmp/pegium-tests/documentation.docs#L6,7)"),
            std::string::npos);
  EXPECT_NE(documentation->find("- `@param p first value`"), std::string::npos);
}

TEST(DefaultDocumentationProviderTest,
     RendersJSDocMarkdownForSingleLineJSDoc) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = parse_docs_document(
      *shared,
      "/** @deprecated Since 1.0 */\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);

  const auto documentation =
      services.documentation.documentationProvider->getDocumentation(
          *model->entries.front());
  ASSERT_TRUE(documentation.has_value());
  EXPECT_EQ(*documentation, "- `@deprecated Since 1.0`");
}

TEST(DefaultDocumentationProviderTest,
     RendersJSDocMarkdownForRootNodeDocumentation) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs",
                                                    {".docs"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = parse_docs_document(
      *shared,
      "/** Root docs. */\n"
      "module Demo\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.documentation.documentationProvider, nullptr);

  const auto documentation =
      services.documentation.documentationProvider->getDocumentation(*model);
  ASSERT_TRUE(documentation.has_value());
  EXPECT_NE(documentation->find("Root docs."), std::string::npos);
}

TEST(DefaultDocumentationProviderTest, ExposesHooksForLinkAndTagRendering) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  auto registeredServices =
      test::make_uninstalled_core_services<DocumentationParser>(*shared, "docs", {".docs"});
  pegium::services::installDefaultCoreServices(*registeredServices);
  registeredServices->documentation.documentationProvider =
      std::make_unique<HookedDocumentationProvider>(*registeredServices);
  shared->serviceRegistry->registerServices(std::move(registeredServices));

  auto document = parse_docs_document(
      *shared,
      "/**\n"
      " * {@link Value|Shown}\n"
      " * @deprecated custom\n"
      " */\n"
      "entry Value\n");
  ASSERT_NE(document, nullptr);
  auto *model =
      dynamic_cast<DocumentationModel *>(document->parseResult.value.get());
  ASSERT_NE(model, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);

  const auto documentation =
      services.documentation.documentationProvider->getDocumentation(
          *model->entries.front());
  ASSERT_TRUE(documentation.has_value());
  EXPECT_NE(documentation->find("<Value|Shown>"), std::string::npos);
  EXPECT_NE(documentation->find("**@deprecated custom**"), std::string::npos);
}

} // namespace
} // namespace pegium::documentation
