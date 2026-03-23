#include <pegium/core/grammar/Create.hpp>

#include <ostream>

namespace pegium::grammar {

void Create::print(std::ostream &os) const {
  os << "{" << getTypeName() << "}";
}

} // namespace pegium::grammar
