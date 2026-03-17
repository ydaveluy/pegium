#pragma once

#include <arithmetics/ast.hpp>

#include <pegium/lsp/AbstractFormatter.hpp>

namespace arithmetics::services::lsp {

class ArithmeticsFormatter : public pegium::lsp::AbstractFormatter {
public:
  explicit ArithmeticsFormatter(const pegium::services::Services &services);

protected:
  virtual void formatModule(pegium::lsp::FormattingBuilder &builder,
                            const ast::Module *module) const;
  virtual void formatDefinition(pegium::lsp::FormattingBuilder &builder,
                                const ast::Definition *definition) const;
  virtual void formatEvaluation(pegium::lsp::FormattingBuilder &builder,
                                const ast::Evaluation *evaluation) const;
  virtual void formatBinaryExpression(pegium::lsp::FormattingBuilder &builder,
                                      const ast::BinaryExpression *binary) const;
  virtual void formatGroupedExpression(
      pegium::lsp::FormattingBuilder &builder,
      const ast::GroupedExpression *grouped) const;
  virtual void formatFunctionCall(pegium::lsp::FormattingBuilder &builder,
                                  const ast::FunctionCall *call) const;
  virtual void formatComment(HiddenNodeFormatter &comment) const;
};

} // namespace arithmetics::services::lsp
