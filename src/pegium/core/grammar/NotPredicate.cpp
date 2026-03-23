#include <pegium/core/grammar/NotPredicate.hpp>

#include <ostream>

namespace pegium::grammar {

void NotPredicate::print(std::ostream &os) const { os << '!' << *getElement(); }

} // namespace pegium::grammar
