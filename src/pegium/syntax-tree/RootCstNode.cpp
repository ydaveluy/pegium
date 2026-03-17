#include <pegium/syntax-tree/RootCstNode.hpp>

#include <algorithm>
#include <limits>
#include <stdexcept>

#include <pegium/workspace/Document.hpp>

namespace pegium {

RootCstNode::RootCstNode(const workspace::Document &document,
                         std::pmr::memory_resource *upstream)
    : _textOwner(document.textDocument()),
      _text(_textOwner != nullptr ? std::addressof(_textOwner->text()) : nullptr),
      _pool(upstream), _document(document) {
  const auto text = getText();
  if (text.size() >
      static_cast<std::size_t>(std::numeric_limits<TextOffset>::max())) {
    throw std::overflow_error(
        "Input text exceeds TextOffset capacity (4 GiB max)");
  }
  _chunks.reserve(std::max<std::size_t>(1, text.size() >> (chunk_shift + 2)));
}

} // namespace pegium
