#include <pegium/lsp/ranges/DefaultSelectionRangeProvider.hpp>
#include <pegium/lsp/support/LspProviderUtils.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <pegium/lsp/services/SharedServices.hpp>

namespace pegium {
using namespace pegium::provider_detail;



std::vector<::lsp::SelectionRange>
DefaultSelectionRangeProvider::getSelectionRanges(
    const workspace::Document &document,
    const ::lsp::SelectionRangeParams &params,
    const utils::CancellationToken &cancelToken) const {
  utils::throw_if_cancelled(cancelToken);
  const auto &textDocument = document.textDocument();
  std::vector<TextOffset> positions;
  positions.reserve(params.positions.size());
  for (const auto &position : params.positions) {
    positions.push_back(textDocument.offsetAt(position));
  }

  std::vector<::lsp::SelectionRange> ranges;
  ranges.reserve(positions.size());
  for (const auto offset : positions) {
    const auto safeOffset =
        std::ranges::min(offset, static_cast<TextOffset>(textDocument.getText().size()));
    ranges.push_back(compute_selection_range(document, safeOffset));
  }
  return ranges;
}

} // namespace pegium
