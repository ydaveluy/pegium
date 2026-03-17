#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <vector>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/grammar/RuleValue.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/syntax-tree/CstNodeView.hpp>

namespace pegium {
struct ReferenceHandle;
}

namespace pegium::parser {
struct ValueBuildContext;
}

namespace pegium::grammar {

struct InfixOperator : AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::InfixOperator;
  }

  enum class Associativity { Left, Right };

  virtual Associativity getAssociativity() const noexcept = 0;
  virtual int32_t getPrecedence() const noexcept = 0;

  virtual const AbstractElement *getOperator() const noexcept = 0;

  virtual RuleValue getValue(const CstNodeView &node) const = 0;
  constexpr ~InfixOperator() noexcept override = default;
  void print(std::ostream &os) const override;
};

struct InfixRule : AbstractRule {
  constexpr ~InfixRule() noexcept override = default;
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::InfixRule;
  }
  virtual std::unique_ptr<AstNode>
  getValue(const CstNodeView &node,
           std::unique_ptr<AstNode> lhsNode,
           const parser::ValueBuildContext &context) const = 0;

  virtual const InfixOperator *
  getOperator(std::size_t index) const noexcept = 0;
  virtual std::size_t operatorCount() const noexcept = 0;
  virtual const AbstractElement *getElement() const noexcept = 0; // TODO rename getElement to getPrimary
  void print(std::ostream &os) const override;
};

} // namespace pegium::grammar
