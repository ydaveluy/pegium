#include <pegium/core/grammar/Literal.hpp>

#include <ostream>
#include <string_view>

#include <pegium/core/syntax-tree/CstNodeView.hpp>
#include <pegium/core/utils/TextUtils.hpp>

namespace pegium::grammar {

std::string_view Literal::getValue(const CstNodeView &node) const noexcept {
  return node.getText();
}

void Literal::print(std::ostream &os) const {
  os << '\'';
  for (const char c : getValue()) {
    os << utils::escape_char(c);
  }
  os << '\'';
  if (!isCaseSensitive()) {
    os << 'i';
  }
}

} // namespace pegium::grammar
