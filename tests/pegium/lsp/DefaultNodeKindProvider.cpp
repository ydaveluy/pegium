#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/symbols/DefaultNodeKindProvider.hpp>

namespace pegium {
namespace {

TEST(DefaultNodeKindProviderTest, ReturnsDefaultKinds) {
  auto shared = test::make_empty_shared_services();
  pegium::services::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_NE(shared->lsp.nodeKindProvider, nullptr);

  workspace::AstNodeDescription description;
  description.name = "symbol";

  EXPECT_EQ(shared->lsp.nodeKindProvider->getSymbolKind(description),
            ::lsp::SymbolKind::Field);
  EXPECT_EQ(shared->lsp.nodeKindProvider->getCompletionItemKind(description),
            ::lsp::CompletionItemKind::Reference);
}

} // namespace
} // namespace pegium
