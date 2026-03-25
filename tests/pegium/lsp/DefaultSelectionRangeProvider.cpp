#include <gtest/gtest.h>

#include <pegium/lsp/LspTestSupport.hpp>
#include <pegium/lsp/ranges/DefaultSelectionRangeProvider.hpp>
#include <pegium/lsp/services/Services.hpp>

namespace pegium {
namespace {

TEST(DefaultSelectionRangeProviderTest, RemainsAvailableAsPegiumOnlyOptInService) {
  auto shared = test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  auto services = test::make_uninstalled_services(*shared, "test", {".test"});
  pegium::installDefaultCoreServices(*services);
  pegium::installDefaultLspServices(*services);

  DefaultSelectionRangeProvider provider(*services);

  workspace::Document document(
      test::make_text_document(test::make_file_uri("selection.test"), "test",
                               "alpha beta"));

  ::lsp::SelectionRangeParams params{};
  params.positions.push_back(text::Position{0, 1});

  const auto ranges =
      provider.getSelectionRanges(document, params, utils::default_cancel_token);
  ASSERT_EQ(ranges.size(), 1u);
  EXPECT_EQ(ranges[0].range.start.line, 0u);
  EXPECT_EQ(ranges[0].range.start.character, 0u);
  EXPECT_EQ(ranges[0].range.end.line, 0u);
  EXPECT_EQ(ranges[0].range.end.character, 5u);
}

} // namespace
} // namespace pegium
