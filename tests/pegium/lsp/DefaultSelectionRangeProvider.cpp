#include <gtest/gtest.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/DefaultSelectionRangeProvider.hpp>
#include <pegium/services/Services.hpp>

namespace pegium::lsp {
namespace {

TEST(DefaultSelectionRangeProviderTest, RemainsAvailableAsPegiumOnlyOptInService) {
  auto shared = test::make_shared_services();
  auto services = test::make_services(*shared, "test", {".test"});

  DefaultSelectionRangeProvider provider(*services);

  workspace::Document document;
  document.uri = test::make_file_uri("selection.test");
  document.languageId = "test";
  document.setText("alpha beta");

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
} // namespace pegium::lsp
