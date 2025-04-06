#include <pegium/grammar/Repetition.hpp>

namespace pegium::grammar {

/*AbstractRepetition::AbstractRepetition(const IGrammarElement *element,
                                       std::size_t min, std::size_t max)
    : _element{element}, _min{min}, _max{max} {}*/

const IGrammarElement *AbstractRepetition::getElement() const noexcept {
  return _element;
}

std::size_t AbstractRepetition::getMin() const noexcept { return _min; }

std::size_t AbstractRepetition::getMax() const noexcept { return _max; }

void AbstractRepetition::print(std::ostream &os) const {
  os << *_element;

  if (_min == 0 && _max == 1)
    os << '?';
  else if (_min == 0 && _max == std::numeric_limits<std::size_t>::max())
    os << '*';
  else if (_min == 1 && _max == std::numeric_limits<std::size_t>::max())
    os << '+';
  else if (_min == _max)
    os << '{' << _min << '}';
  else if (_max == std::numeric_limits<std::size_t>::max())
    os << '{' << _min << ",}";
  else
    os << '{' << _min << ',' << _max << '}';
}

GrammarElementKind AbstractRepetition::getKind() const noexcept {
  return GrammarElementKind::Repetition;
}

} // namespace pegium::grammar