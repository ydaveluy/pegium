#include <pegium/grammar/AbstractElement.hpp>

#include <ostream>

namespace pegium::grammar {

std::ostream &operator<<(std::ostream &os, const AbstractElement &obj) {
  obj.print(os);
  return os;
}

} // namespace pegium::grammar
