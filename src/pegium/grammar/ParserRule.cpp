#include <pegium/grammar/ParserRule.hpp>

#include <ostream>

namespace pegium::grammar {

void ParserRule::print(std::ostream &os) const {

  os  << getName() << " returns " << getTypeName() << ": "
     << *getElement() << ";";
}

} // namespace pegium::grammar
