#include <pegium/core/syntax-tree/RootCstNode.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

namespace pegium {

RootCstNode::RootCstNode(text::TextSnapshot text,
                         std::pmr::memory_resource *upstream)
    : _text(std::move(text)), _pool(upstream) {
  const auto view = getText();
  if (view.size() >
      static_cast<std::size_t>(std::numeric_limits<TextOffset>::max())) {
    throw std::overflow_error(
        "Input text exceeds TextOffset capacity (4 GiB max)");
  }
  _chunks.reserve(std::max<std::size_t>(1, view.size() >> (chunk_shift + 2)));
}

} // namespace pegium
