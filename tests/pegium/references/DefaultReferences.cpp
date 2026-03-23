#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/syntax-tree/CstUtils.hpp>

namespace pegium::references {
namespace {

using namespace pegium::parser;

struct ReferenceEntry : AstNode {
  string name;
};

class ReferenceParser final : public PegiumParser {
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
  Rule<ReferenceEntry> EntryRule{"Entry",
                                 "entry"_kw + assign<&ReferenceEntry::name>(ID)};
#pragma clang diagnostic pop
};

class CanonicalReferenceParser final : public PegiumParser {
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

  Terminal<std::string> ID{
      "ID", "a-zA-Z_"_cr + many(w),
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<std::string> {
        std::string value(text);
        std::ranges::transform(value, value.begin(), [](unsigned char c) {
          return static_cast<char>(std::tolower(c));
        });
        return opt::conversion_value<std::string>(std::move(value));
      })};
  Rule<ReferenceEntry> EntryRule{"Entry",
                                 "entry"_kw + assign<&ReferenceEntry::name>(ID)};
#pragma clang diagnostic pop
};

TEST(DefaultReferencesTest, FindsDeclarationAtEndOfFileIdentifierOffset) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<ReferenceParser>(*shared, "ref", {".ref"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("reference.ref"), "ref", "entry Alpha");
  ASSERT_NE(document, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.references.references, nullptr);
  ASSERT_NE(document->parseResult.cst, nullptr);

  const auto selectedNode = find_declaration_node_at_offset(
      *document->parseResult.cst,
      static_cast<TextOffset>(document->textDocument().getText().size()));
  ASSERT_TRUE(selectedNode.has_value());
  const auto declarations =
      services.references.references->findDeclarations(*selectedNode);
  ASSERT_EQ(declarations.size(), 1u);
  const auto *entry = dynamic_cast<const ReferenceEntry *>(declarations.front());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->name, "Alpha");
}

TEST(DefaultReferencesTest,
     FindsDeclarationAtDeclarationSiteEvenWhenStoredNameIsCanonicalized) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_core_services<CanonicalReferenceParser>(*shared, "ref",
                                                         {".ref"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("canonical-reference.ref"), "ref",
      "entry BasicMath");
  ASSERT_NE(document, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.references.references, nullptr);
  ASSERT_NE(document->parseResult.cst, nullptr);

  const auto selectedNode = find_declaration_node_at_offset(
      *document->parseResult.cst,
      static_cast<TextOffset>(document->textDocument().getText().find("BasicMath") +
                              1));
  ASSERT_TRUE(selectedNode.has_value());
  const auto declarations =
      services.references.references->findDeclarations(*selectedNode);
  ASSERT_EQ(declarations.size(), 1u);
  const auto *entry = dynamic_cast<const ReferenceEntry *>(declarations.front());
  ASSERT_NE(entry, nullptr);
  EXPECT_EQ(entry->name, "basicmath");
}

TEST(DefaultReferencesTest, DoesNotTreatKeywordAsDeclarationSite) {
  auto shared = test::make_empty_shared_core_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  {
    auto registeredServices =
        test::make_uninstalled_core_services<ReferenceParser>(*shared, "ref", {".ref"});
    pegium::services::installDefaultCoreServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("keyword-site.ref"), "ref", "entry Alpha");
  ASSERT_NE(document, nullptr);

  const auto &services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services.references.references, nullptr);
  ASSERT_NE(document->parseResult.cst, nullptr);

  const auto selectedNode =
      find_declaration_node_at_offset(*document->parseResult.cst, 1u);
  ASSERT_TRUE(selectedNode.has_value());
  EXPECT_TRUE(services.references.references->findDeclarations(*selectedNode).empty());
}

} // namespace
} // namespace pegium::references
