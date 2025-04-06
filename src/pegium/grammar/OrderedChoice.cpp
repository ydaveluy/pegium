
#include <pegium/grammar/OrderedChoice.hpp>

namespace pegium::grammar {

const IGrammarElement *const *AbstractOrderedChoice::begin() const noexcept {
  return _begin;
}
const IGrammarElement *const *AbstractOrderedChoice::end() const noexcept {
  return _end;
}
GrammarElementKind AbstractOrderedChoice::getKind() const noexcept {
  return GrammarElementKind::OrderedChoice;
}
void AbstractOrderedChoice::print(std::ostream &os) const {
  os << '(';

  for (auto it = _begin; it != _end; ++it) {
    if (it != _begin)
      os << " | ";
    os << **it;
  }
  os << ')';
}

} // namespace pegium::grammar