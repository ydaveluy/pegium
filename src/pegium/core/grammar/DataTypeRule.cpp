#include <pegium/core/grammar/DataTypeRule.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void DataTypeRule::print(std::ostream &os) const {
  os << getName() << " returns " << getTypeName() << ": ";
  detail::print_element_reference(os, *getElement());
  os << ';';
}

} // namespace pegium::grammar
