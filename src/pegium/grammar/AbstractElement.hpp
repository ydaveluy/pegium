#pragma once
#include <ostream>
namespace pegium::grammar {

enum class ElementKind {

  New,  // an Action that create a new instance
  Init, // an action that create a new instance and init a feature with current
        // value
  Assignment, // assign a feature of the current element
  AndPredicate,
  AnyCharacter,
  CharacterRange,
  DataTypeRule,
  Group,
  Literal,
  NotPredicate,
  OrderedChoice,
  ParserRule,
  Repetition,
  TerminalRule,
  UnorderedGroup,
  CrossReference
};

struct AbstractElement {
  using ElementKind = pegium::grammar::ElementKind;
  constexpr virtual ElementKind getKind() const noexcept = 0;
  constexpr virtual ~AbstractElement() noexcept = default;
  constexpr virtual void print(std::ostream &os) const = 0;

  friend std::ostream &operator<<(std::ostream &os,
                                  const AbstractElement &obj) {
    obj.print(os);
    return os;
  }
};

} // namespace pegium::grammar