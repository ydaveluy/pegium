#pragma once

#include <arithmetics/ast.hpp>

#include <pegium/lsp/formatting/AbstractFormatter.hpp>

namespace arithmetics::lsp {

class ArithmeticsFormatter : public pegium::AbstractFormatter {
public:
  explicit ArithmeticsFormatter(const pegium::Services &services);

protected:
  virtual void formatModule(pegium::FormattingBuilder &builder,
                            const ast::Module *module) const;
  virtual void formatDefinition(pegium::FormattingBuilder &builder,
                                const ast::Definition *definition) const;
  virtual void formatEvaluation(pegium::FormattingBuilder &builder,
                                const ast::Evaluation *evaluation) const;
  virtual void formatBinaryExpression(pegium::FormattingBuilder &builder,
                                      const ast::BinaryExpression *binary) const;
  virtual void formatGroupedExpression(
      pegium::FormattingBuilder &builder,
      const ast::GroupedExpression *grouped) const;
  virtual void formatFunctionCall(pegium::FormattingBuilder &builder,
                                  const ast::FunctionCall *call) const;
  virtual void formatComment(HiddenNodeFormatter &comment) const;
};

} // namespace arithmetics::lsp
