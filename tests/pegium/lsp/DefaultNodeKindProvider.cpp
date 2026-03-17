#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultNodeKindProvider.hpp>

namespace pegium::lsp {
namespace {

TEST(DefaultNodeKindProviderTest, ReturnsDefaultKinds) {
  auto shared = test::make_shared_services();
  ASSERT_NE(shared->lsp.nodeKindProvider, nullptr);

  workspace::AstNodeDescription description;
  description.name = "symbol";

  EXPECT_EQ(shared->lsp.nodeKindProvider->getSymbolKind(description),
            ::lsp::SymbolKind::Field);
  EXPECT_EQ(shared->lsp.nodeKindProvider->getCompletionItemKind(description),
            ::lsp::CompletionItemKind::Reference);
}

} // namespace
} // namespace pegium::lsp
