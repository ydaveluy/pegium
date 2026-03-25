#include <pegium/core/grammar/InfixRule.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void InfixOperator::print(std::ostream &os) const {

  switch (getAssociativity()) {
  case Associativity::Left:
    os << "left";
    break;
  case Associativity::Right:
    os << "right";
    break;
  }

  os << " assoc ";
  detail::print_element_reference(os, *getOperator());
}

void InfixRule::print(std::ostream &os) const {
  os << "infix " << getName() << " returns " << getTypeName() << " on ";
  detail::print_element_reference(os, *getElement());
  os << ": ";
  for (std::size_t i = 0; i < operatorCount(); ++i) {
    if (i > 0)
      os << " > ";
    os << *getOperator(i);
  }
  os << ";";
}

} // namespace pegium::grammar
