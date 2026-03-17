#pragma once

#include <memory>
#include <vector>
#include <pegium/grammar/AbstractRule.hpp>
#include <pegium/syntax-tree/Reference.hpp>

namespace pegium {
class AstNode;
class CstNodeView;
}

namespace pegium::parser {
struct ParseContext;
struct TrackedParseContext;
struct RecoveryContext;
struct ExpectContext;
struct ValueBuildContext;
}

namespace pegium::grammar {

struct ParserRule : AbstractRule {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::ParserRule;
  }
  virtual std::unique_ptr<AstNode>
  getValue(const CstNodeView &, const parser::ValueBuildContext &) const = 0;
  virtual bool rule(parser::ParseContext &) const = 0;
  virtual bool rule(parser::TrackedParseContext &) const = 0;
  virtual bool recover(parser::RecoveryContext &) const = 0;
  virtual bool expect(parser::ExpectContext &) const = 0;
  virtual const AbstractElement *getElement() const noexcept = 0;

  void print(std::ostream &os) const override;
};
template <typename T>
concept IsParserRule = std::derived_from<std::remove_cvref_t<T>, ParserRule>;

} // namespace pegium::grammar
