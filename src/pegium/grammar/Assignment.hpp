#pragma once
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/syntax-tree.hpp>
#include <string_view>

namespace pegium::grammar {

enum class AssignmentOperator { Assign, Append, EnableIf };

inline constexpr std::ostream &operator<<(std::ostream &os,
                                          const AssignmentOperator &op) {
  switch (op) {
  case AssignmentOperator::Assign:
    return os << '=';
  case AssignmentOperator::Append:
    return os << "+=";
  case AssignmentOperator::EnableIf:
    return os << "?=";
  }
  return os;
}
struct Assignment : AbstractElement {
  using AssignmentOperator = pegium::grammar::AssignmentOperator;
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Assignment;
  }
  virtual constexpr AssignmentOperator getOperator() const noexcept {
    return AssignmentOperator::Append;
  }
  constexpr ~Assignment() noexcept override = default;
  virtual void execute(AstNode *current, const CstNode &node) const = 0;
  virtual const AbstractElement *getElement() const noexcept = 0;
  virtual std::string_view getFeature() const noexcept = 0;
  void print(std::ostream &os) const final {
    os << getFeature() << getOperator() << *getElement();
  }
};

} // namespace pegium::grammar