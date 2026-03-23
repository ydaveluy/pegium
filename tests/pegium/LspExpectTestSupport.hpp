#pragma once

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <concepts>
#include <functional>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <lsp/types.h>

#include <pegium/LspTestSupport.hpp>
#include <pegium/lsp/runtime/internal/LanguageServerFeatureDispatch.hpp>
#include <pegium/lsp/services/ServiceAccess.hpp>

namespace pegium::test {

inline const pegium::Services *
lookup_services(const pegium::SharedServices &sharedServices,
                std::string_view languageId) {
  for (const auto *coreServices : sharedServices.serviceRegistry->all()) {
    if (coreServices != nullptr &&
        coreServices->languageMetaData.languageId == languageId) {
      const auto *services = as_services(coreServices);
      if (services != nullptr) {
        return services;
      }
    }
  }
  return nullptr;
}

struct ParseHelperOptions {
  std::optional<std::string> documentUri;
};

inline std::shared_ptr<workspace::Document>
parseDocument(pegium::SharedServices &sharedServices,
              std::string_view languageId, std::string text,
              const ParseHelperOptions &options = {}) {
  static std::uint32_t nextDocumentId = 1;

  const auto *services = lookup_services(sharedServices, languageId);
  if (services == nullptr) {
    ADD_FAILURE() << "No services registered for language " << languageId;
    return nullptr;
  }

  std::string uri;
  if (options.documentUri.has_value()) {
    uri = *options.documentUri;
  } else {
    std::string extension;
    if (!services->languageMetaData.fileExtensions.empty()) {
      extension = services->languageMetaData.fileExtensions.front();
    } else {
      extension = ".test";
    }
    uri = make_file_uri("helper-" + std::to_string(nextDocumentId++) + extension);
  }

  return open_and_build_document(sharedServices, std::move(uri),
                                 std::string(languageId), std::move(text));
}

struct ExpectedBase {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
};

struct ReplacedIndices {
  std::string output;
  std::vector<TextOffset> indices;
  std::vector<std::pair<TextOffset, TextOffset>> ranges;
};

template <typename Expected>
ReplacedIndices replaceIndices(const Expected &base) {
  ReplacedIndices result;
  result.output.reserve(base.text.size());
  std::vector<TextOffset> rangeStack;

  for (std::size_t index = 0; index < base.text.size();) {
    if (!base.indexMarker.empty() &&
        base.text.compare(index, base.indexMarker.size(), base.indexMarker) == 0) {
      result.indices.push_back(static_cast<TextOffset>(result.output.size()));
      index += base.indexMarker.size();
      continue;
    }
    if (!base.rangeStartMarker.empty() &&
        base.text.compare(index, base.rangeStartMarker.size(),
                          base.rangeStartMarker) == 0) {
      rangeStack.push_back(static_cast<TextOffset>(result.output.size()));
      index += base.rangeStartMarker.size();
      continue;
    }
    if (!base.rangeEndMarker.empty() &&
        base.text.compare(index, base.rangeEndMarker.size(), base.rangeEndMarker) ==
            0) {
      EXPECT_FALSE(rangeStack.empty());
      if (!rangeStack.empty()) {
        result.ranges.emplace_back(rangeStack.back(),
                                   static_cast<TextOffset>(result.output.size()));
        rangeStack.pop_back();
      }
      index += base.rangeEndMarker.size();
      continue;
    }
    result.output.push_back(base.text[index++]);
  }

  EXPECT_TRUE(rangeStack.empty());
  return result;
}

inline ::lsp::TextDocumentPositionParams
textDocumentPositionParams(const workspace::Document &document, TextOffset offset) {
  ::lsp::TextDocumentPositionParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document.uri));
  params.position = document.textDocument().positionAt(offset);
  return params;
}

inline ::lsp::CompletionParams
completionParams(const workspace::Document &document, TextOffset offset) {
  ::lsp::CompletionParams params{};
  params.textDocument = textDocumentPositionParams(document, offset).textDocument;
  params.position = textDocumentPositionParams(document, offset).position;
  return params;
}

inline ::lsp::DefinitionParams definitionParams(const workspace::Document &document,
                                                TextOffset offset) {
  ::lsp::DefinitionParams params{};
  params.textDocument = textDocumentPositionParams(document, offset).textDocument;
  params.position = textDocumentPositionParams(document, offset).position;
  return params;
}

inline ::lsp::HoverParams hoverParams(const workspace::Document &document,
                                      TextOffset offset) {
  ::lsp::HoverParams params{};
  params.textDocument = textDocumentPositionParams(document, offset).textDocument;
  params.position = textDocumentPositionParams(document, offset).position;
  return params;
}

inline ::lsp::DocumentHighlightParams
highlightParams(const workspace::Document &document, TextOffset offset) {
  ::lsp::DocumentHighlightParams params{};
  params.textDocument = textDocumentPositionParams(document, offset).textDocument;
  params.position = textDocumentPositionParams(document, offset).position;
  return params;
}

inline ::lsp::ReferenceParams
referenceParams(const workspace::Document &document, TextOffset offset,
                bool includeDeclaration) {
  ::lsp::ReferenceParams params{};
  params.textDocument.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document.uri));
  params.position = document.textDocument().positionAt(offset);
  params.context.includeDeclaration = includeDeclaration;
  return params;
}

inline std::string applyTextEdits(const workspace::Document &document,
                                  std::vector<::lsp::TextEdit> edits) {
  const auto &textDocument = document.textDocument();
  auto text = std::string(textDocument.getText());
  std::ranges::sort(edits, [&document](const auto &left, const auto &right) {
    return document.textDocument().offsetAt(left.range.start) >
           document.textDocument().offsetAt(right.range.start);
  });

  for (const auto &edit : edits) {
    const auto begin = textDocument.offsetAt(edit.range.start);
    const auto end = textDocument.offsetAt(edit.range.end);
    text.replace(begin, end - begin, edit.newText);
  }
  return text;
}

inline std::string hoverText(const ::lsp::Hover &hover) {
  return std::visit(
      []<typename T>(const T &value) -> std::string {
        using Value = std::remove_cvref_t<T>;
        if constexpr (std::same_as<Value, ::lsp::MarkupContent>) {
          return value.value;
        } else if constexpr (std::same_as<Value, ::lsp::MarkedString>) {
          return std::holds_alternative<std::string>(value)
                     ? std::get<std::string>(value)
                     : std::get<::lsp::MarkedString_Language_Value>(value).value;
        } else {
          std::string combined;
          for (const auto &entry : value) {
            if (!combined.empty()) {
              combined += '\n';
            }
            combined += hoverText(::lsp::Hover{.contents = entry});
          }
          return combined;
        }
      },
      hover.contents);
}

inline bool samePosition(const ::lsp::Position &actual,
                         const text::Position &expected) {
  return actual.line == expected.line &&
         actual.character == expected.character;
}

struct ExpectedSymbols {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::vector<std::string> expectedSymbols;
  std::function<void(const std::vector<::lsp::DocumentSymbol> &)> check;
  std::function<std::string(const ::lsp::DocumentSymbol &)> symbolToString;
};

inline std::shared_ptr<workspace::Document>
expectSymbols(pegium::SharedServices &sharedServices,
              std::string_view languageId, const ExpectedSymbols &expected) {
  auto document =
      parseDocument(sharedServices, languageId, expected.text,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  const auto symbols = pegium::getDocumentSymbols(
      sharedServices,
      ::lsp::DocumentSymbolParams{
          .textDocument =
              {.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri))}});

  if (expected.check) {
    expected.check(symbols);
    return document;
  }

  const auto symbolToString = expected.symbolToString
                                  ? expected.symbolToString
                                  : [](const auto &symbol) { return symbol.name; };
  if (symbols.size() != expected.expectedSymbols.size()) {
    ADD_FAILURE() << "Expected " << expected.expectedSymbols.size()
                  << " document symbols but found " << symbols.size();
    return document;
  }
  for (std::size_t index = 0; index < expected.expectedSymbols.size(); ++index) {
    EXPECT_EQ(symbolToString(symbols[index]), expected.expectedSymbols[index]);
  }
  return document;
}

struct ExpectedWorkspaceSymbols {
  std::string query;
  std::vector<std::string> expectedSymbols;
  std::function<void(const std::vector<::lsp::WorkspaceSymbol> &)> check;
  std::function<std::string(const ::lsp::WorkspaceSymbol &)> symbolToString;
};

inline void expectWorkspaceSymbols(
    pegium::SharedServices &sharedServices,
    const ExpectedWorkspaceSymbols &expected) {
  const auto symbols = pegium::getWorkspaceSymbols(
      sharedServices, ::lsp::WorkspaceSymbolParams{.query = expected.query});

  if (expected.check) {
    expected.check(symbols);
    return;
  }

  const auto symbolToString = expected.symbolToString
                                  ? expected.symbolToString
                                  : [](const auto &symbol) { return symbol.name; };
  if (symbols.size() != expected.expectedSymbols.size()) {
    ADD_FAILURE() << "Expected " << expected.expectedSymbols.size()
                  << " workspace symbols but found " << symbols.size();
    return;
  }
  for (std::size_t index = 0; index < expected.expectedSymbols.size(); ++index) {
    EXPECT_EQ(symbolToString(symbols[index]), expected.expectedSymbols[index]);
  }
}

struct ExpectedFoldings {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::function<void(
      const std::vector<::lsp::FoldingRange> &,
      const std::vector<std::pair<TextOffset, TextOffset>> &)>
      check;
};

inline std::shared_ptr<workspace::Document>
expectFoldings(pegium::SharedServices &sharedServices,
               std::string_view languageId, const ExpectedFoldings &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  auto ranges = pegium::getFoldingRanges(
      sharedServices,
      ::lsp::FoldingRangeParams{
          .textDocument =
              {.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri))}});
  std::ranges::sort(ranges, [](const auto &left, const auto &right) {
    return left.startLine < right.startLine;
  });

  if (expected.check) {
    expected.check(ranges, marked.ranges);
    return document;
  }

  if (ranges.size() != marked.ranges.size()) {
    ADD_FAILURE() << "Expected " << marked.ranges.size()
                  << " folding ranges but found " << ranges.size();
    return document;
  }
  for (std::size_t index = 0; index < marked.ranges.size(); ++index) {
    const auto [begin, end] = marked.ranges[index];
    EXPECT_EQ(ranges[index].startLine,
              document->textDocument().positionAt(begin).line);
    EXPECT_EQ(ranges[index].endLine,
              document->textDocument().positionAt(end).line);
  }
  return document;
}

struct ExpectedCompletion {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::size_t index = 0;
  std::vector<std::string> expectedItems;
  std::function<void(const ::lsp::CompletionList &)> check;
  std::function<std::string(const ::lsp::CompletionItem &)> itemToString;
};

inline std::shared_ptr<workspace::Document>
expectCompletion(pegium::SharedServices &sharedServices,
                 std::string_view languageId, const ExpectedCompletion &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  if (expected.index >= marked.indices.size()) {
    ADD_FAILURE() << "Completion marker index " << expected.index
                  << " is out of range.";
    return document;
  }
  auto completion = pegium::getCompletion(
      sharedServices, completionParams(*document, marked.indices[expected.index]));
  if (!completion.has_value()) {
    ADD_FAILURE() << "No completion result was produced.";
    return document;
  }

  if (expected.check) {
    expected.check(*completion);
    return document;
  }

  auto items = completion->items;
  std::ranges::sort(items, [](const auto &left, const auto &right) {
    const auto leftSort = left.sortText.value_or(left.label);
    const auto rightSort = right.sortText.value_or(right.label);
    return leftSort < rightSort;
  });
  const auto itemToString =
      expected.itemToString ? expected.itemToString
                            : [](const auto &item) { return item.label; };
  if (items.size() != expected.expectedItems.size()) {
    ADD_FAILURE() << "Expected " << expected.expectedItems.size()
                  << " completion items but found " << items.size();
    return document;
  }
  for (std::size_t index = 0; index < expected.expectedItems.size(); ++index) {
    EXPECT_EQ(itemToString(items[index]), expected.expectedItems[index]);
  }
  return document;
}

struct ExpectedGoToDefinition {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::size_t index = 0;
  std::variant<std::monostate, std::size_t, std::vector<std::size_t>> rangeIndex;
};

inline std::shared_ptr<workspace::Document>
expectGoToDefinition(pegium::SharedServices &sharedServices,
                     std::string_view languageId,
                     const ExpectedGoToDefinition &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  if (expected.index >= marked.indices.size()) {
    ADD_FAILURE() << "Definition marker index " << expected.index
                  << " is out of range.";
    return document;
  }
  auto links = pegium::getDefinition(
      sharedServices, definitionParams(*document, marked.indices[expected.index]));
  if (!links.has_value()) {
    ADD_FAILURE() << "No definition result was produced.";
    return document;
  }

  std::vector<std::size_t> expectedRanges;
  if (std::holds_alternative<std::vector<std::size_t>>(expected.rangeIndex)) {
    expectedRanges = std::get<std::vector<std::size_t>>(expected.rangeIndex);
  } else if (std::holds_alternative<std::size_t>(expected.rangeIndex)) {
    expectedRanges.push_back(std::get<std::size_t>(expected.rangeIndex));
  } else {
    expectedRanges.resize(marked.ranges.size());
    for (std::size_t index = 0; index < marked.ranges.size(); ++index) {
      expectedRanges[index] = index;
    }
  }

  if (links->size() != expectedRanges.size()) {
    ADD_FAILURE() << "Expected " << expectedRanges.size()
                  << " definitions but found " << links->size();
    return document;
  }
  for (std::size_t index = 0; index < expectedRanges.size(); ++index) {
    const auto [begin, end] = marked.ranges[expectedRanges[index]];
    EXPECT_TRUE(samePosition((*links)[index].targetSelectionRange.start,
                             document->textDocument().positionAt(begin)));
    EXPECT_TRUE(samePosition((*links)[index].targetSelectionRange.end,
                             document->textDocument().positionAt(end)));
  }
  return document;
}

struct ExpectedFindReferences {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  bool includeDeclaration = false;
};

inline std::shared_ptr<workspace::Document>
expectFindReferences(pegium::SharedServices &sharedServices,
                     std::string_view languageId,
                     const ExpectedFindReferences &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  for (const auto offset : marked.indices) {
    const auto references = pegium::getReferences(
        sharedServices, referenceParams(*document, offset,
                                        expected.includeDeclaration));
    if (references.size() != marked.ranges.size()) {
      ADD_FAILURE() << "Expected " << marked.ranges.size()
                    << " references but found " << references.size();
      return document;
    }
    for (const auto &reference : references) {
      const auto match = std::ranges::any_of(
          marked.ranges, [&](const auto &expectedRange) {
            return samePosition(reference.range.start,
                                document->textDocument().positionAt(
                                    expectedRange.first)) &&
                   samePosition(reference.range.end,
                                document->textDocument().positionAt(
                                    expectedRange.second));
          });
      EXPECT_TRUE(match);
    }
  }

  return document;
}

struct ExpectedHover {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::size_t index = 0;
  std::optional<std::string> hover;
  std::function<void(const std::optional<::lsp::Hover> &)> check;
};

inline std::shared_ptr<workspace::Document>
expectHover(pegium::SharedServices &sharedServices,
            std::string_view languageId, const ExpectedHover &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  if (expected.index >= marked.indices.size()) {
    ADD_FAILURE() << "Hover marker index " << expected.index
                  << " is out of range.";
    return document;
  }
  const auto hover = pegium::getHoverContent(
      sharedServices, hoverParams(*document, marked.indices[expected.index]));

  if (expected.check) {
    expected.check(hover);
    return document;
  }

  if (!expected.hover.has_value()) {
    EXPECT_FALSE(hover.has_value());
    return document;
  }

  if (!hover.has_value()) {
    ADD_FAILURE() << "No hover result was produced.";
    return document;
  }
  EXPECT_EQ(hoverText(*hover), *expected.hover);
  return document;
}

struct ExpectedHighlight {
  std::string text;
  std::optional<std::string> documentUri;
  std::string indexMarker = "<|>";
  std::string rangeStartMarker = "<|";
  std::string rangeEndMarker = "|>";
  std::size_t index = 0;
  std::variant<std::monostate, std::size_t, std::vector<std::size_t>> rangeIndex;
};

inline std::shared_ptr<workspace::Document>
expectHighlight(pegium::SharedServices &sharedServices,
                std::string_view languageId,
                const ExpectedHighlight &expected) {
  const auto marked = replaceIndices(expected);
  auto document =
      parseDocument(sharedServices, languageId, marked.output,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  if (expected.index >= marked.indices.size()) {
    ADD_FAILURE() << "Highlight marker index " << expected.index
                  << " is out of range.";
    return document;
  }
  const auto highlights = pegium::getDocumentHighlights(
      sharedServices, highlightParams(*document, marked.indices[expected.index]));

  std::vector<std::size_t> expectedRanges;
  if (std::holds_alternative<std::vector<std::size_t>>(expected.rangeIndex)) {
    expectedRanges = std::get<std::vector<std::size_t>>(expected.rangeIndex);
  } else if (std::holds_alternative<std::size_t>(expected.rangeIndex)) {
    expectedRanges.push_back(std::get<std::size_t>(expected.rangeIndex));
  } else {
    expectedRanges.resize(marked.ranges.size());
    for (std::size_t index = 0; index < marked.ranges.size(); ++index) {
      expectedRanges[index] = index;
    }
  }

  if (highlights.size() != expectedRanges.size()) {
    ADD_FAILURE() << "Expected " << expectedRanges.size()
                  << " highlights but found " << highlights.size();
    return document;
  }
  for (std::size_t index = 0; index < expectedRanges.size(); ++index) {
    const auto [begin, end] = marked.ranges[expectedRanges[index]];
    EXPECT_TRUE(
        samePosition(highlights[index].range.start,
                     document->textDocument().positionAt(begin)));
    EXPECT_TRUE(
        samePosition(highlights[index].range.end,
                     document->textDocument().positionAt(end)));
  }
  return document;
}

struct ExpectFormatting {
  std::string before;
  std::string after;
  std::optional<::lsp::Range> range;
  ::lsp::FormattingOptions options{
      .tabSize = 4,
      .insertSpaces = true,
  };
  std::optional<std::string> documentUri;
};

inline std::shared_ptr<workspace::Document>
expectFormatting(pegium::SharedServices &sharedServices,
                 std::string_view languageId,
                 const ExpectFormatting &expected) {
  auto document =
      parseDocument(sharedServices, languageId, expected.before,
                    ParseHelperOptions{.documentUri = expected.documentUri});
  if (document == nullptr) {
    return nullptr;
  }

  std::vector<::lsp::TextEdit> edits;
  if (expected.range.has_value()) {
    edits = pegium::formatDocumentRange(
        sharedServices,
        ::lsp::DocumentRangeFormattingParams{
            .textDocument =
                {.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri))},
            .range = *expected.range,
            .options = expected.options,
        });
  } else {
    edits = pegium::formatDocument(
        sharedServices,
        ::lsp::DocumentFormattingParams{
            .textDocument =
                {.uri = ::lsp::DocumentUri(::lsp::Uri::parse(document->uri))},
            .options = expected.options,
        });
  }

  EXPECT_EQ(applyTextEdits(*document, std::move(edits)), expected.after);
  return document;
}

} // namespace pegium::test
