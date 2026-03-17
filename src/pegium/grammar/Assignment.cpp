#include <pegium/grammar/Assignment.hpp>

#include <ostream>

namespace pegium::grammar {

std::ostream &operator<<(std::ostream &os, const AssignmentOperator &op) {
  switch (op) {
  case AssignmentOperator::Assign:
    return os << '=';
  case AssignmentOperator::Append:
    return os << "+=";
  case AssignmentOperator::EnableIf:
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
