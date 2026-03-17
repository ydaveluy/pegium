#include <pegium/grammar/Repetition.hpp>

#include <cstddef>
#include <limits>
#include <ostream>

namespace pegium::grammar {

void Repetition::print(std::ostream &os) const {
  const auto min = getMin();
  const auto max = getMax();
  os << *getElement();

  if (min == 0 && max == 1) {
    os << '?';
  } else if (min == 0 && max == std::numeric_limits<std::size_t>::max()) {
    os << '*';
  } else if (min == 1 && max == std::numeric_limits<std::size_t>::max()) {
    os << '+';
  } else if (min == max) {
    os << '{' << min << '}';
  } else if (max == std::numeric_limits<std::size_t>::max()) {
    os << '{' << min << ",}";
  } else {
    os << '{' << min << ',' << max << '}';
  }
}

} // namespace pegium::grammar
