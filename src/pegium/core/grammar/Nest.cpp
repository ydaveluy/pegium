#include <pegium/core/grammar/Nest.hpp>

#include <ostream>

namespace pegium::grammar {

void Nest::print(std::ostream &os) const {
  os << "{" << getTypeName() << "." << getFeature() << "=current}";
}

} // namespace pegium::grammar
