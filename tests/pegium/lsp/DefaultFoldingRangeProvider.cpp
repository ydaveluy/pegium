#include <gtest/gtest.h>

#include <memory>

#include <pegium/LspTestSupport.hpp>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {
namespace {

using namespace pegium::parser;

TEST(DefaultFoldingRangeProviderTest, ReturnsVisibleRangesSpanningMultipleLines) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "test", {".test"})));

  workspace::Document document;
  document.uri = test::make_file_uri("folding.test");
  document.languageId = "test";
  document.setText("line1\nline2\nline3");

  auto root = std::make_unique<RootCstNode>(document);
  CstBuilder builder(*root);
  static constexpr pegium::parser::Literal<std::array{'r', 'e', 'g', 'i', 'o',
                                                      'n'}>
      region{};
  builder.leaf(0, static_cast<pegium::TextOffset>(document.text().size()),
               std::addressof(region));
  document.parseResult.cst = std::move(root);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("test");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  const auto ranges = services->lsp.foldingRangeProvider->getFoldingRanges(
      document, ::lsp::FoldingRangeParams{});
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].startLine, 0u);
  EXPECT_GT(ranges[0].endLine, ranges[0].startLine);
}

TEST(DefaultFoldingRangeProviderTest, SkipsHiddenNodes) {
  auto shared = test::make_shared_services();
  ASSERT_TRUE(shared->serviceRegistry->registerServices(
      test::make_services(*shared, "test", {".test"})));

  workspace::Document document;
  document.uri = test::make_file_uri("hidden-folding.test");
  document.languageId = "test";
  document.setText("line1\nline2");

  auto root = std::make_unique<RootCstNode>(document);
  CstBuilder builder(*root);
  static constexpr pegium::parser::Literal<std::array{'h', 'i', 'd', 'd', 'e',
                                                      'n'}>
      hidden{};
  builder.leaf(0, static_cast<pegium::TextOffset>(document.text().size()),
               std::addressof(hidden), true);
  document.parseResult.cst = std::move(root);

  const auto *coreServices =
      shared->serviceRegistry->getServicesByLanguageId("test");
  ASSERT_NE(coreServices, nullptr);
  const auto *services = dynamic_cast<const services::Services *>(coreServices);
  ASSERT_NE(services, nullptr);

  EXPECT_TRUE(services->lsp.foldingRangeProvider
                  ->getFoldingRanges(document, ::lsp::FoldingRangeParams{})
                  .empty());
}

} // namespace
} // namespace pegium::lsp
