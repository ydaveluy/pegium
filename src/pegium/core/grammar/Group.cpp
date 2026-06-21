#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void Group::print(std::ostream &os) const { detail::print_nary(os, *this, " "); }

} // namespace pegium::grammar
