#include <pegium/lsp/ranges/DefaultFoldingRangeProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>
#include <pegium/core/utils/TransparentStringHash.hpp>

namespace pegium {
using namespace pegium::provider_detail;



std::vector<::lsp::FoldingRange>
DefaultFoldingRangeProvider::getFoldingRanges(
    const workspace::Document &document,
    const ::lsp::FoldingRangeParams &params,
    const utils::CancellationToken &cancelToken) const {
  (void)params;
  utils::throw_if_cancelled(cancelToken);
  std::vector<FoldingRangeData> rawRanges;
  assert(document.parseResult.cst != nullptr);
  const auto &textDocument = document.textDocument();

  utils::TransparentStringSet seen;
  for (const auto &child : *document.parseResult.cst) {
    collect_folding_ranges(child, textDocument.getText(), rawRanges, seen);
  }

  std::ranges::sort(rawRanges, [](const FoldingRangeData &left,
                                  const FoldingRangeData &right) {
    return left.begin < right.begin;
  });

  std::vector<::lsp::FoldingRange> ranges;
  ranges.reserve(rawRanges.size());
  for (const auto &range : rawRanges) {
    const auto start = textDocument.positionAt(range.begin);
    const auto end = textDocument.positionAt(range.end);
    if (end.line <= start.line) {
      continue;
    }

    ::lsp::FoldingRange folding{};
    folding.startLine = start.line;
    folding.endLine = end.line;
    folding.startCharacter = start.character;
    folding.endCharacter = end.character;
    folding.kind = range.kind;
    ranges.push_back(std::move(folding));
  }
  return ranges;
}

} // namespace pegium
