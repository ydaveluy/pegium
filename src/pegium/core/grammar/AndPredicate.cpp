#include <pegium/core/grammar/AndPredicate.hpp>

#include <ostream>

namespace pegium::grammar {

void AndPredicate::print(std::ostream &os) const { os << '&' << *getElement(); }

} // namespace pegium::grammar
