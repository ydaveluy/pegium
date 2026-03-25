#include <pegium/core/grammar/AndPredicate.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void AndPredicate::print(std::ostream &os) const {
  os << '&';
  detail::print_element_reference(os, *getElement());
}

} // namespace pegium::grammar
