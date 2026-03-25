#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void ParserRule::print(std::ostream &os) const {
  os << getName() << " returns " << getTypeName() << ": ";
  detail::print_element_reference(os, *getElement());
  os << ';';
}

} // namespace pegium::grammar
