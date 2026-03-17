#include <gtest/gtest.h>

#include <pegium/workspace/TextDocument.hpp>

namespace pegium::workspace {
namespace {

TEST(TextDocumentTest, ReplaceTextUpdatesRevisionAndOffsets) {
  TextDocument document;
  document.replaceText("alpha\nbeta");

  EXPECT_EQ(document.revision(), 1u);
  EXPECT_EQ(document.positionToOffset(text::Position(1, 2)), 8u);
  const auto position = document.offsetToPosition(8u);
  EXPECT_EQ(position.line, 1u);
  EXPECT_EQ(position.character, 2u);
}

TEST(TextDocumentTest, ApplyContentChangesRebuildsTextAndLineIndex) {
  TextDocument document;
  document.replaceText("alpha\nbeta");

  const TextDocumentContentChange change{
      .range = TextDocumentContentChangeRange(text::Position(1, 0),
                                              text::Position(1, 4)),
      .text = "gamma",
  };

  document.applyContentChanges(std::span<const TextDocumentContentChange>(&change, 1));

  EXPECT_EQ(document.text(), "alpha\ngamma");
  EXPECT_EQ(document.revision(), 2u);
  EXPECT_EQ(document.positionToOffset(text::Position(1, 4)), 10u);
}

} // namespace
} // namespace pegium::workspace
