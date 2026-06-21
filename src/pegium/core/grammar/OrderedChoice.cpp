#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void OrderedChoice::print(std::ostream &os) const {
  detail::print_nary(os, *this, " | ");
}

} // namespace pegium::grammar
