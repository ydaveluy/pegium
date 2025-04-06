#include <pegium/grammar/AndPredicate.hpp>

namespace pegium::grammar {

const IGrammarElement *AbstractAndPredicate::getElement() const noexcept {
  return _element;
}
void AbstractAndPredicate::print(std::ostream &os) const {
  os << '&' << *_element;
}

GrammarElementKind AbstractAndPredicate::getKind() const noexcept {
  return GrammarElementKind::AndPredicate;
}

} // namespace pegium::grammar