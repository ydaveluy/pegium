#include <gtest/gtest.h>

#include <array>
#include <stdexcept>

#include <pegium/core/workspace/DocumentFactory.hpp>
#include <pegium/core/workspace/TextDocument.hpp>

namespace pegium::workspace {
namespace {

class SnapshottingDocumentFactory final : public DocumentFactory {
public:
  using DocumentFactory::snapshot;

  [[nodiscard]] std::shared_ptr<Document> fromTextDocument(
      std::shared_ptr<TextDocument>,
      const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<Document>
  fromString(std::string, std::string,
             const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  [[nodiscard]] std::shared_ptr<Document>
  fromUri(std::string_view,
          const utils::CancellationToken & = {}) const override {
    throw std::logic_error("Not used in this test helper.");
  }

  Document &update(Document &document,
                   const utils::CancellationToken & = {}) const override {
    return document;
  }
};

TEST(TextDocumentTest, CreateInitializesVersionAndOffsets) {
  auto document = TextDocument::create("file:///text-document.test", "test", 1,
                                       "alpha\nbeta");

  EXPECT_EQ(document.uri(), "file:///text-document.test");
  EXPECT_EQ(document.languageId(), "test");
  EXPECT_EQ(document.version(), 1);
  EXPECT_EQ(document.lineCount(), 2u);
  EXPECT_EQ(document.offsetAt(text::Position(1, 2)), 8u);
  const auto position = document.positionAt(8u);
  EXPECT_EQ(position.line, 1u);
  EXPECT_EQ(position.character, 2u);
}

TEST(TextDocumentTest, UpdateAppliesIncrementalChangesAndVersion) {
  auto document = TextDocument::create("file:///text-document.test", "test", 1,
                                       "alpha\nbeta");
  const std::array<TextDocumentContentChangeEvent, 1> changes{{
      {.range = text::Range(text::Position(1, 0), text::Position(1, 4)),
       .rangeLength = 4u,
       .text = "gamma"},
  }};

  (void)TextDocument::update(document, changes, 2);

  EXPECT_EQ(document.getText(), "alpha\ngamma");
  EXPECT_EQ(document.version(), 2);
  EXPECT_EQ(document.offsetAt(text::Position(1, 4)), 10u);
}

TEST(TextDocumentTest, EmptyDocumentMapsOffsetZeroToOrigin) {
  auto document =
      TextDocument::create("file:///empty-text-document.test", "test", 0, "");

  const auto position = document.positionAt(0u);

  EXPECT_EQ(position.line, 0u);
  EXPECT_EQ(position.character, 0u);
  EXPECT_EQ(document.lineCount(), 1u);
}

TEST(TextDocumentTest, PositionAtUsesUtf16ColumnsOnSingleLineUtf8Text) {
  auto document =
      TextDocument::create("file:///utf8-text-document.test", "test", 1,
                           "entry \"Café\"");

  const auto accentPosition = document.positionAt(12u);
  EXPECT_EQ(accentPosition.line, 0u);
  EXPECT_EQ(accentPosition.character, 11u);

  const auto endPosition = document.positionAt(13u);
  EXPECT_EQ(endPosition.line, 0u);
  EXPECT_EQ(endPosition.character, 12u);
}

TEST(TextDocumentTest, PositionAtUsesUtf16ColumnsForEmojiOnSingleLineUtf8Text) {
  auto document =
      TextDocument::create("file:///emoji-text-document.test", "test", 1,
                           "entry \"😀\"");

  const auto emojiStart = document.positionAt(7u);
  EXPECT_EQ(emojiStart.line, 0u);
  EXPECT_EQ(emojiStart.character, 7u);

  const auto emojiEnd = document.positionAt(11u);
  EXPECT_EQ(emojiEnd.line, 0u);
  EXPECT_EQ(emojiEnd.character, 9u);

  const auto endPosition = document.positionAt(12u);
  EXPECT_EQ(endPosition.line, 0u);
  EXPECT_EQ(endPosition.character, 10u);
}

TEST(TextDocumentTest, ApplyEditsReturnsUpdatedTextWithoutMutatingSource) {
  const auto document = TextDocument::create("file:///apply-edits.test", "test",
                                             1, "alpha\nbeta");
  const std::array<TextEdit, 1> edits{{
      {.range = text::Range(text::Position(1, 0), text::Position(1, 4)),
       .newText = "gamma"},
  }};

  const auto updated = TextDocument::applyEdits(document, edits);

  EXPECT_EQ(updated, "alpha\ngamma");
  EXPECT_EQ(document.getText(), "alpha\nbeta");
}

TEST(TextDocumentTest, GetTextRangeClampsAndSwapsBounds) {
  const auto document = TextDocument::create("file:///range-text-document.test",
                                             "test", 1, "alpha\nbeta");

  EXPECT_EQ(document.getText(
                text::Range(text::Position(1, 4), text::Position(1, 1))),
            "eta");
}

TEST(TextDocumentTest, InternalSnapshotRemainsValidAfterInPlaceUpdate) {
  SnapshottingDocumentFactory factory;
  auto document =
      TextDocument::create("file:///snapshot-text-document.test", "test", 1,
                           "alpha\nbeta");
  const auto snapshot = factory.snapshot(document);
  const std::array<TextDocumentContentChangeEvent, 1> changes{{
      {.text = "gamma\ndelta"},
  }};

  (void)TextDocument::update(document, changes, 2);

  EXPECT_EQ(snapshot.str(), "alpha\nbeta");
  EXPECT_EQ(document.getText(), "gamma\ndelta");
  EXPECT_EQ(document.version(), 2);
}

} // namespace
} // namespace pegium::workspace
