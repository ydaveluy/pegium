#include <pegium/lsp/DefaultDocumentHighlightProvider.hpp>
#include <pegium/lsp/LspProviderUtils.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <pegium/services/SharedServices.hpp>

namespace pegium::lsp {

using namespace detail;

namespace {

::lsp::Range make_range(const workspace::Document &document, TextOffset begin,
                        TextOffset end) {
  ::lsp::Range range{};
  range.start = document.offsetToPosition(begin);
  range.end = document.offsetToPosition(end);
  return range;
}

} // namespace

std::vector<::lsp::DocumentHighlight>
DefaultDocumentHighlightProvider::getDocumentHighlight(
    const workspace::Document &document,
    const ::lsp::DocumentHighlightParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto offset = document.positionToOffset(params.position);
  const auto *references = languageServices.references.references.get();
  if (references == nullptr) {
    return {};
  }

  std::vector<::lsp::DocumentHighlight> highlights;
  std::unordered_set<std::string> seen;

  if (const auto declaration = references->findDeclarationAt(document, offset);
      declaration.has_value() && declaration->documentId == document.id) {
    if (declaration->nameLength != 0) {
      DocumentHighlightData key{
          .begin = declaration->offset,
          .end = static_cast<TextOffset>(declaration->offset +
                                         declaration->nameLength),
          .kind = ::lsp::DocumentHighlightKind::Write,
      };
      if (seen.insert(document_highlight_key(key)).second) {
        ::lsp::DocumentHighlight highlight{};
        highlight.range = make_range(document, key.begin, key.end);
        highlight.kind = key.kind;
        highlights.push_back(std::move(highlight));
      }
    }
  }

  for (const auto &reference : references->findReferencesAt(document, offset, false)) {
    utils::throw_if_cancelled(cancelToken);
    if (!std::holds_alternative<workspace::ReferenceDescription>(reference)) {
      continue;
    }
    const auto &usage = std::get<workspace::ReferenceDescription>(reference);
    if (usage.sourceDocumentId != document.id) {
      continue;
    }
    DocumentHighlightData key{
        .begin = usage.sourceOffset,
        .end = static_cast<TextOffset>(usage.sourceOffset + usage.sourceLength),
        .kind = ::lsp::DocumentHighlightKind::Read,
    };
    if (seen.insert(document_highlight_key(key)).second) {
      ::lsp::DocumentHighlight highlight{};
      highlight.range = make_range(document, key.begin, key.end);
      highlight.kind = key.kind;
      highlights.push_back(std::move(highlight));
    }
  }

  std::ranges::sort(highlights, [](const ::lsp::DocumentHighlight &left,
                                   const ::lsp::DocumentHighlight &right) {
    if (left.range.start.line == right.range.start.line) {
      return left.range.start.character < right.range.start.character;
    }
    return left.range.start.line < right.range.start.line;
  });
  return highlights;
}

} // namespace pegium::lsp
