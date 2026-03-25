#include <pegium/core/grammar/TerminalRule.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void TerminalRule::print(std::ostream &os) const {
  os << "terminal " << getName() << " returns " << getTypeName() << ": ";
  detail::print_element_reference(os, *getElement());
  os << ';';
}

} // namespace pegium::grammar
