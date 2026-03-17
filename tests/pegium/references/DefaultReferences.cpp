#include <gtest/gtest.h>

#include <pegium/CoreTestSupport.hpp>
#include <pegium/parser/PegiumParser.hpp>

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
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services<ReferenceParser>(*shared, "ref", {".ref"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("reference.ref"), "ref", "entry Alpha");
  ASSERT_NE(document, nullptr);

  const auto *services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->references.references, nullptr);

  const auto declaration = services->references.references->findDeclarationAt(
      *document, static_cast<TextOffset>(document->text().size()));
  ASSERT_TRUE(declaration.has_value());
  EXPECT_EQ(declaration->name, "Alpha");
}

TEST(DefaultReferencesTest,
     FindsDeclarationAtDeclarationSiteEvenWhenStoredNameIsCanonicalized) {
  auto shared = test::make_shared_core_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_core_services<CanonicalReferenceParser>(*shared, "ref",
                                                         {".ref"})));

  auto document = test::open_and_build_document(
      *shared, test::make_file_uri("canonical-reference.ref"), "ref",
      "entry BasicMath");
  ASSERT_NE(document, nullptr);

  const auto *services = shared->serviceRegistry->getServices(document->uri);
  ASSERT_NE(services, nullptr);
  ASSERT_NE(services->references.references, nullptr);

  const auto declaration = services->references.references->findDeclarationAt(
      *document, static_cast<TextOffset>(document->text().find("BasicMath") + 1));
  ASSERT_TRUE(declaration.has_value());
  EXPECT_EQ(declaration->name, "basicmath");
}

} // namespace
} // namespace pegium::references
