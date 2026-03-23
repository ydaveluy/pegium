#include <pegium/core/grammar/DataTypeRule.hpp>

#include <ostream>

namespace pegium::grammar {

void DataTypeRule::print(std::ostream &os) const {

  os  << getName() << " returns " << getTypeName() << ": "
     << *getElement() << ";";
}

} // namespace pegium::grammar
