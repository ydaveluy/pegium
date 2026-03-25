#include <pegium/core/grammar/NotPredicate.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void NotPredicate::print(std::ostream &os) const {
  os << '!';
  detail::print_element_reference(os, *getElement());
}

} // namespace pegium::grammar
