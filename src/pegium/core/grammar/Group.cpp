#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <cstddef>
#include <ostream>

namespace pegium::grammar {

void Group::print(std::ostream &os) const {
  os << '(';
  const auto elementCount = size();
  for (std::size_t elementIndex = 0; elementIndex < elementCount;
       ++elementIndex) {
    if (elementIndex > 0) {
      os << ' ';
    }
    detail::print_element_reference(os, *get(elementIndex));
  }
  os << ')';
}

} // namespace pegium::grammar
