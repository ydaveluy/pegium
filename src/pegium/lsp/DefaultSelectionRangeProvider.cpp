#include <pegium/lsp/DefaultSelectionRangeProvider.hpp>
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

std::vector<::lsp::SelectionRange>
DefaultSelectionRangeProvider::getSelectionRanges(
    const workspace::Document &document,
    const ::lsp::SelectionRangeParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  std::vector<TextOffset> positions;
  positions.reserve(params.positions.size());
  for (const auto &position : params.positions) {
    positions.push_back(document.positionToOffset(position));
  }

  std::vector<::lsp::SelectionRange> ranges;
  ranges.reserve(positions.size());
  for (const auto offset : positions) {
    const auto safeOffset =
        std::ranges::min(offset, static_cast<TextOffset>(document.text().size()));
    ranges.push_back(compute_selection_range(document, safeOffset));
  }
  return ranges;
}

} // namespace pegium::lsp
