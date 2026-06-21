#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/grammar/PrintUtils.hpp>

#include <ostream>

namespace pegium::grammar {

void ParserRule::print(std::ostream &os) const { detail::print_rule(os, *this); }

} // namespace pegium::grammar
