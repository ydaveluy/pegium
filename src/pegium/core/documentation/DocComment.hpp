#pragma once

/// Structured documentation comment model with a tokenizer, parser and Markdown/plain
/// renderers. This mirrors the behaviour of a documentation comment such as:
///
///     /**
///      * A paragraph of text with an {@link Target inline link}.
///      * @param value first value
///      */
///
/// Comments are parsed into a tree of paragraphs, tags and lines, each carrying
/// the source range it occupies. The comment markers (`/**`, `*`, `*/`) are
/// configurable through `DocCommentParseOptions`.

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::documentation {

/// Zero-based line/character position inside the source document.
struct DocCommentPosition {
  std::uint32_t line = 0;
  std::uint32_t character = 0;
};

/// Half-open source range `[start, end)`.
struct DocCommentRange {
  DocCommentPosition start;
  DocCommentPosition end;
};

/// Options controlling how a comment is delimited.
///
/// Markers are matched as literal text: `start`/`line` after leading
/// whitespace at the beginning of a line, `end` (with surrounding whitespace)
/// at the end of the last line.
struct DocCommentParseOptions {
  std::string start = "/**";
  std::string line = "*";
  std::string end = "*/";
};

/// Options controlling how a parsed comment is rendered to Markdown.
struct DocCommentRenderOptions {
  /// Emphasis applied to a `@tag` marker. Defaults to italic.
  enum class TagStyle : std::uint8_t { Plain, Italic, Bold, BoldItalic };
  TagStyle tag = TagStyle::Italic;

  /// Default rendering for `{@link}` targets. Defaults to plain.
  enum class LinkStyle : std::uint8_t { Plain, Code };
  LinkStyle link = LinkStyle::Plain;

  /// Custom tag renderer. Receives the tag name, its rendered content and
  /// whether it is inline. Return the Markdown to use, or `std::nullopt` to
  /// fall back to the default rendering.
  std::function<std::optional<std::string>(
      std::string_view name, std::string_view content, bool inlineTag)>
      renderTag;

  /// Custom link renderer for inline `{@link target display}` tags. Return the
  /// Markdown link, or `std::nullopt` to fall back to the default rendering.
  std::function<std::optional<std::string>(std::string_view target,
                                           std::string_view display)>
      renderLink;
};

/// One node of a parsed documentation comment.
///
/// A node is one of:
/// - `Paragraph`: a block of text whose `children` are the inline nodes (lines
///   and inline tags) it is composed of.
/// - `Tag`: a `@tag`; `children` hold its content (lines and inline tags). An
///   inline tag (`{@link ...}`) has `inlineTag == true`.
/// - `Line`: a single line of plain text in `text`.
struct DocCommentNode {
  enum class Kind : std::uint8_t { Paragraph, Tag, Line };

  Kind kind = Kind::Line;
  DocCommentRange range;

  /// Plain text content of a `Line` node.
  std::string text;

  /// Tag name (without the leading `@`) of a `Tag` node.
  std::string name;
  /// Whether a `Tag` node is an inline `{@tag ...}` rather than a block `@tag`.
  bool inlineTag = false;

  /// Inline nodes of a `Paragraph`, or the content of a `Tag`.
  std::vector<DocCommentNode> children;

  [[nodiscard]] std::string toString() const;
  [[nodiscard]] std::string toMarkdown(const DocCommentRenderOptions &options) const;
};

/// A fully parsed documentation comment.
struct DocComment {
  std::vector<DocCommentNode> elements;
  DocCommentRange range;

  /// Returns the first block/inline tag named `name`, or `nullptr`.
  [[nodiscard]] const DocCommentNode *getTag(std::string_view name) const;
  /// Returns every tag named `name`, in document order.
  [[nodiscard]] std::vector<const DocCommentNode *>
  getTags(std::string_view name) const;

  /// Renders the comment to plain text.
  [[nodiscard]] std::string toString() const;
  /// Renders the comment to Markdown.
  [[nodiscard]] std::string toMarkdown(const DocCommentRenderOptions &options = {}) const;
};

/// Returns whether `comment` is delimited as a documentation comment under `options`
/// (its first line starts with `start` and its last line ends with `end`).
[[nodiscard]] bool is_doc_comment(std::string_view comment,
                            const DocCommentParseOptions &options = {});

/// Parses `comment` into a structured documentation model.
///
/// `start` is the position the comment occupies in the source document, used to
/// anchor every node range; it defaults to the document origin.
[[nodiscard]] DocComment parse_doc_comment(std::string_view comment,
                                       DocCommentPosition start = {},
                                       const DocCommentParseOptions &options = {});

} // namespace pegium::documentation
