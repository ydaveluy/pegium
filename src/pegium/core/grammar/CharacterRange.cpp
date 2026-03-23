#include <pegium/core/grammar/CharacterRange.hpp>

#include <string_view>

#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

std::string_view
CharacterRange::getValue(const CstNodeView &node) const noexcept {
  return node.getText();
}

} // namespace pegium::grammar
