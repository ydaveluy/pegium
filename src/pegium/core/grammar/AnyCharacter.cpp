#include <pegium/core/grammar/AnyCharacter.hpp>

#include <ostream>
#include <string_view>

#include <pegium/core/syntax-tree/CstNodeView.hpp>

namespace pegium::grammar {

std::string_view AnyCharacter::getValue(const CstNodeView &node) const noexcept {
  return node.getText();
}

void AnyCharacter::print(std::ostream &os) const { os << '.'; }

} // namespace pegium::grammar
