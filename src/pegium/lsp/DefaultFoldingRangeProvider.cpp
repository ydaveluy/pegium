#include <pegium/lsp/DefaultFoldingRangeProvider.hpp>
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
#include <pegium/utils/TransparentStringHash.hpp>

namespace pegium::lsp {

using namespace detail;

std::vector<::lsp::FoldingRange>
DefaultFoldingRangeProvider::getFoldingRanges(
    const workspace::Document &document,
    const ::lsp::FoldingRangeParams &params,
    const utils::CancellationToken &cancelToken) const {
  (void)params;
  utils::throw_if_cancelled(cancelToken);
  std::vector<FoldingRangeData> rawRanges;
  if (document.parseResult.cst == nullptr) {
    return {};
  }

  utils::TransparentStringSet seen;
  for (const auto &child : *document.parseResult.cst) {
    collect_folding_ranges(child, document.textView(), rawRanges, seen);
  }

  std::ranges::sort(rawRanges, [](const FoldingRangeData &left,
                                  const FoldingRangeData &right) {
    return left.begin < right.begin;
  });

  std::vector<::lsp::FoldingRange> ranges;
  ranges.reserve(rawRanges.size());
  for (const auto &range : rawRanges) {
    const auto start = document.offsetToPosition(range.begin);
    const auto end = document.offsetToPosition(range.end);
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

} // namespace pegium::lsp
