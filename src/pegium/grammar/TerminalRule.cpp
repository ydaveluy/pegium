#include <pegium/grammar/TerminalRule.hpp>

#include <ostream>

namespace pegium::grammar {

void TerminalRule::print(std::ostream &os) const {

  os << "terminal " << getName() << " returns " << getTypeName() << ": "
     << *getElement() << ";";
}

} // namespace pegium::grammar
