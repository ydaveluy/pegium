#pragma once
#include <pegium/grammar/AbstractElement.hpp>
#include <pegium/grammar/FeatureValue.hpp>
#include <pegium/grammar/RuleValue.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>
#include <memory>
#include <string_view>
#include <typeindex>
#include <variant>

namespace pegium::parser {
struct ValueBuildContext;
}

namespace pegium::grammar {

enum class AssignmentOperator { Assign, Append, EnableIf };

std::ostream &operator<<(std::ostream &os, const AssignmentOperator &op);

struct Assignment : AbstractElement {
  using AssignmentOperator = pegium::grammar::AssignmentOperator;
  constexpr ElementKind getKind() const noexcept final {
    return ElementKind::Assignment;
  }
  virtual AssignmentOperator getOperator() const noexcept;
  constexpr ~Assignment() noexcept override = default;
  virtual void execute(AstNode *current, const CstNodeView &cst,
                       const parser::ValueBuildContext &context) const = 0;
  virtual FeatureValue getValue(const AstNode *ast) const = 0;
  virtual const AbstractElement *getElement() const noexcept = 0;
  virtual std::string_view getFeature() const noexcept = 0;
  [[nodiscard]] virtual bool isReference() const noexcept { return false; }
  [[nodiscard]] virtual bool isMultiReference() const noexcept { return false; }
  [[nodiscard]] virtual std::type_index getReferenceType() const noexcept {
    return std::type_index(typeid(void));
  }
  [[nodiscard]] virtual bool
  acceptsReferenceTarget(const AstNode *node) const noexcept {
    (void)node;
    return false;
  }
  void print(std::ostream &os) const final;
};

} // namespace pegium::grammar
