#include <pegium/documentation/DefaultCommentProvider.hpp>
#include <pegium/grammar/TerminalRule.hpp>

namespace pegium::documentation {

namespace {

bool is_multiline_comment(const CstNodeView &node) noexcept {
  if (!node.isHidden()) {
    return false;
  }
  const auto *grammarElement = node.getGrammarElement();
  return grammarElement->getKind() == grammar::ElementKind::TerminalRule &&
         static_cast<const grammar::TerminalRule *>(grammarElement)->getName() ==
             "ML_COMMENT";
}

} // namespace

std::string_view
DefaultCommentProvider::getComment(const AstNode &node) const noexcept {
  auto cst = node.getCstNode();
  if (!cst)
    return {};

  const auto begin = cst.getBegin();
  for (auto previous = cst.previous(); previous; previous = previous.previous()) {
    // Pegium stores CST nodes in a flat pre-order array. Skip nodes that still
    // overlap the current node span, as they belong to the current subtree or
    // its enclosing composite nodes. The first node that ends before the
    // current node begins is the lexical predecessor we want here.
    if (previous.getEnd() > begin) {
      continue;
    }
    if (is_multiline_comment(previous)) {
      return previous.getText();
    }
    break;
  }
  return {};
}
} // namespace pegium::documentation
