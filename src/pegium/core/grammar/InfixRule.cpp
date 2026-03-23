#include <pegium/core/grammar/InfixRule.hpp>

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

  os << " assoc " << *getOperator();
}

void InfixRule::print(std::ostream &os) const {
  os << "infix " << getName() << " returns " << getTypeName() << " on ";
  os << *getElement() << ": ";
  for (std::size_t i = 0; i < operatorCount(); ++i) {
    if (i > 0)
      os << " > ";
    os << *getOperator(i);
  }
  os << ";";
}

} // namespace pegium::grammar
