#include <pegium/grammar/UnorderedGroup.hpp>

#include <cstddef>
#include <ostream>

namespace pegium::grammar {

void UnorderedGroup::print(std::ostream &os) const {
  os << '(';
  const auto elementCount = size();
  for (std::size_t elementIndex = 0; elementIndex < elementCount;
       ++elementIndex) {
    if (elementIndex > 0) {
      os << " & ";
    }
    os << *get(elementIndex);
  }
  os << ')';
}

} // namespace pegium::grammar
