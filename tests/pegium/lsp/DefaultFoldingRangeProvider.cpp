#include <gtest/gtest.h>

#include <memory>

#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {
namespace {

using pegium::as_services;
using namespace pegium::parser;

TEST(DefaultFoldingRangeProviderTest, ReturnsVisibleRangesSpanningMultipleLines) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  workspace::Document document(
      test::make_text_document(test::make_file_uri("folding.test"), "test",
                               "line1\nline2\nline3"));

  auto root = std::make_unique<RootCstNode>(text::TextSnapshot::copy(document.textDocument().getText()));
  root->attachDocument(document);
  CstBuilder builder(*root);
  static constexpr pegium::parser::Literal<std::array{'r', 'e', 'g', 'i', 'o',
                                                      'n'}>
      region{};
  builder.leaf(0,
               static_cast<pegium::TextOffset>(document.textDocument().getText().size()),
               std::addressof(region));
  document.parseResult.cst = std::move(root);

  const auto *coreServices = &shared->serviceRegistry->getServices(document.uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);

  const auto ranges = services->lsp.foldingRangeProvider->getFoldingRanges(
      document, ::lsp::FoldingRangeParams{});
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].startLine, 0u);
  EXPECT_GT(ranges[0].endLine, ranges[0].startLine);
}

TEST(DefaultFoldingRangeProviderTest, SkipsHiddenNodes) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  {
    auto registeredServices = 
      test::make_uninstalled_services(*shared, "test", {".test"});
    pegium::installDefaultCoreServices(*registeredServices);
    pegium::installDefaultLspServices(*registeredServices);
    shared->serviceRegistry->registerServices(std::move(registeredServices));
  }

  workspace::Document document(test::make_text_document(
      test::make_file_uri("hidden-folding.test"), "test", "line1\nline2"));

  auto root = std::make_unique<RootCstNode>(text::TextSnapshot::copy(document.textDocument().getText()));
  root->attachDocument(document);
  CstBuilder builder(*root);
  static constexpr pegium::parser::Literal<std::array{'h', 'i', 'd', 'd', 'e',
                                                      'n'}>
      hidden{};
  builder.leaf(0,
               static_cast<pegium::TextOffset>(document.textDocument().getText().size()),
               std::addressof(hidden), true);
  document.parseResult.cst = std::move(root);

  const auto *coreServices = &shared->serviceRegistry->getServices(document.uri);
  const auto *services = as_services(coreServices);
  ASSERT_NE(services, nullptr);

  EXPECT_TRUE(services->lsp.foldingRangeProvider
                  ->getFoldingRanges(document, ::lsp::FoldingRangeParams{})
                  .empty());
}

} // namespace
} // namespace pegium
