#include <pegium/core/grammar/Assignment.hpp>

#include <ostream>

namespace pegium::grammar {

std::ostream &operator<<(std::ostream &os, const AssignmentOperator &op) {
  using enum AssignmentOperator;
  switch (op) {
  case Assign:
    return os << '=';
  case Append:
    return os << "+=";
  case EnableIf:
    return os << "?=";
  }
  return os;
}

Assignment::AssignmentOperator Assignment::getOperator() const noexcept {
  return AssignmentOperator::Append;
}

void Assignment::print(std::ostream &os) const {
  os << getFeature() << getOperator() << *getElement();
}

} // namespace pegium::grammar
