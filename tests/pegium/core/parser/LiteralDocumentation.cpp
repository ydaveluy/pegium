#include <gtest/gtest.h>

#include <optional>
#include <string>

#include <pegium/core/documentation/DocumentationProvider.hpp>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/Literal.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>

namespace pegium {
namespace {

using namespace pegium::parser;

TEST(LiteralDocumentationTest, DocAttachesAndRoundTrips) {
  const auto documented = "class"_kw.doc("A class component.");
  EXPECT_EQ(documented.getDocumentation(), "A class component.");

  // A plain keyword carries no documentation.
  EXPECT_TRUE("class"_kw.getDocumentation().empty());

  // The documentation is reachable through the grammar base — the exact path
  // the DocumentationProvider and hover take from a CST node's grammar element.
  const grammar::Literal &base = documented;
  EXPECT_EQ(base.getDocumentation(), "A class component.");
}

struct StubDocumentationProvider final : documentation::DocumentationProvider {
  using documentation::DocumentationProvider::getDocumentation;
  std::optional<std::string> getDocumentation(const AstNode &) const override {
    return std::nullopt;
  }
};

TEST(LiteralDocumentationTest, ProviderReturnsKeywordDocumentation) {
  const StubDocumentationProvider provider;

  const auto documented = "class"_kw.doc("A class component.");
  const grammar::AbstractElement &element = documented;
  EXPECT_EQ(provider.getDocumentation(element),
            std::optional<std::string>{"A class component."});

  // An undocumented keyword yields no documentation.
  const auto plain = "class"_kw;
  const grammar::AbstractElement &plainElement = plain;
  EXPECT_FALSE(provider.getDocumentation(plainElement).has_value());
}

} // namespace
} // namespace pegium
