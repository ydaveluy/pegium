#include <pegium/grammar/CharacterRange.hpp>

#include <string_view>

#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

std::string_view
CharacterRange::getValue(const CstNodeView &node) const noexcept {
  return node.getText();
}

} // namespace pegium::grammar
