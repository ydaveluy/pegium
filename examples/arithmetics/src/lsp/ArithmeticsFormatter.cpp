#include "lsp/ArithmeticsFormatter.hpp"

namespace arithmetics::services::lsp {
void ArithmeticsFormatter::formatModule(pegium::lsp::FormattingBuilder &builder,
                                        const ast::Module *module) const {
  auto formatter = builder.getNodeFormatter(module);
  formatter.keyword("module").prepend(noIndent).append(oneSpace);
  formatter.properties<&ast::Module::statements>().prepend(newLine);
}

void ArithmeticsFormatter::formatDefinition(
    pegium::lsp::FormattingBuilder &builder,
    const ast::Definition *definition) const {
  auto formatter = builder.getNodeFormatter(definition);
  formatter.keyword("def").append(oneSpace);

  if (!definition->args.empty()) {
    auto openParen = formatter.keyword("(");
    auto closeParen = formatter.keyword(")");
    openParen.prepend(noSpace).append(noSpace);
    closeParen.prepend(noSpace);
    formatSeparatedList(formatter.keywords(","));
  }

  formatter.keyword(":").prepend(noSpace).append(fit(oneSpace, indent));
  formatter.keyword(";").prepend(noSpace);
}

void ArithmeticsFormatter::formatEvaluation(
    pegium::lsp::FormattingBuilder &builder,
    const ast::Evaluation *evaluation) const {
  auto formatter = builder.getNodeFormatter(evaluation);
  formatter.keyword(";").prepend(noSpace);
}

void ArithmeticsFormatter::formatBinaryExpression(
    pegium::lsp::FormattingBuilder &builder,
    const ast::BinaryExpression *binary) const {
  if (!binary->left || !binary->right) {
    return;
  }

  auto formatter = builder.getNodeFormatter(binary);
  formatter.keyword(binary->op).surround(oneSpace);
}

void ArithmeticsFormatter::formatGroupedExpression(
    pegium::lsp::FormattingBuilder &builder,
    const ast::GroupedExpression *grouped) const {
  if (!grouped->expression) {
    return;
  }

  auto formatter = builder.getNodeFormatter(grouped);
  auto openParen = formatter.keyword("(");
  auto closeParen = formatter.keyword(")");
  openParen.append(noSpace);
  closeParen.prepend(noSpace);
}

void ArithmeticsFormatter::formatFunctionCall(
    pegium::lsp::FormattingBuilder &builder,
    const ast::FunctionCall *call) const {
  if (call->args.empty()) {
    return;
  }

  auto formatter = builder.getNodeFormatter(call);
  auto openParen = formatter.keyword("(");
  auto closeParen = formatter.keyword(")");
  openParen.prepend(noSpace).append(noSpace);
  closeParen.prepend(noSpace);
  formatSeparatedList(formatter.keywords(","));
}

void ArithmeticsFormatter::formatComment(HiddenNodeFormatter &comment) const {
  comment.replace(formatMultilineComment(comment));
}

ArithmeticsFormatter::ArithmeticsFormatter(
    const pegium::services::Services &services)
    : AbstractFormatter(services) {
  on<ast::Module>(&ArithmeticsFormatter::formatModule);
  on<ast::Definition>(&ArithmeticsFormatter::formatDefinition);
  on<ast::Evaluation>(&ArithmeticsFormatter::formatEvaluation);
  on<ast::BinaryExpression>(&ArithmeticsFormatter::formatBinaryExpression);
  on<ast::GroupedExpression>(&ArithmeticsFormatter::formatGroupedExpression);
  on<ast::FunctionCall>(&ArithmeticsFormatter::formatFunctionCall);
  onHidden("ML_COMMENT", &ArithmeticsFormatter::formatComment);
}

} // namespace arithmetics::services::lsp
