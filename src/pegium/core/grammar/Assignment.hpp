#pragma once
/// Grammar contract for AST feature assignments and references.
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/FeatureValue.hpp>
#include <pegium/core/grammar/RuleValue.hpp>
#include <pegium/core/syntax-tree/AstNode.hpp>
#include <pegium/core/syntax-tree/CstNodeView.hpp>
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
  [[nodiscard]] virtual bool isReference() const noexcept = 0;
  [[nodiscard]] virtual bool isMultiReference() const noexcept = 0;
  [[nodiscard]] virtual std::type_index getType() const noexcept = 0;
  void print(std::ostream &os) const final;
};

} // namespace pegium::grammar
