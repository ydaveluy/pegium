#include <pegium/grammar/Assignment.hpp>

namespace pegium::grammar {

const IGrammarElement *AbstractAssignment::getElement() const noexcept {
  return _element;
};
std::string_view AbstractAssignment::getName() const noexcept { return _name; };

void AbstractAssignment::print(std::ostream &os) const {
  os << getName() << "=" << *_element;
}

GrammarElementKind AbstractAssignment::getKind() const noexcept {
  return GrammarElementKind::Assignment;
}

} // namespace pegium::grammar