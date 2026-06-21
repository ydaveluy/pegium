#include <pegium/core/grammar/UnorderedGroup.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void UnorderedGroup::print(std::ostream &os) const {
  detail::print_nary(os, *this, " & ");
}

} // namespace pegium::grammar
