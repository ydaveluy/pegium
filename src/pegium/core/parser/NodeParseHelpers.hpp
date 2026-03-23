#pragma once

/// AST node construction helpers shared by parser rules and assignments.

#include <utility>

#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/parser/ParseExpression.hpp>
#include <pegium/core/parser/ParseMode.hpp>

namespace pegium::parser::detail {

template <typename Context, typename ParseFn>
  requires std::invocable<ParseFn &>
[[gnu::always_inline]] inline bool
parse_wrapped_node(Context &ctx, const grammar::AbstractElement *element,
                   ParseFn &&parse_fn) {
  const auto nodeStartCheckpoint = ctx.enter();
  if (!parse_fn()) {
    return false;
  }
  ctx.exit(nodeStartCheckpoint, element);
  return true;
}

template <typename Context, Expression E>
[[gnu::always_inline]] inline bool
parse_wrapped_node(Context &ctx, const grammar::AbstractElement *element,
                   const E &expression) {
  const auto nodeStartCheckpoint = ctx.enter();
  if (!parse(expression, ctx)) {
    return false;
  }
  ctx.exit(nodeStartCheckpoint, element);
  return true;
}

template <typename Context, typename ParseFn>
  requires std::invocable<ParseFn &>
[[gnu::always_inline]] inline bool parse_overriding_first_child(
    Context &ctx, const grammar::AbstractElement *element, ParseFn &&parse_fn) {
  const auto nodeCountBefore = ctx.node_count();
  if (!parse_fn()) {
    return false;
  }
  if (ctx.node_count() > nodeCountBefore) {
    ctx.override_grammar_element(nodeCountBefore, element);
  }
  return true;
}

template <typename Context, Expression E>
[[gnu::always_inline]] inline bool parse_overriding_first_child(
    Context &ctx, const grammar::AbstractElement *element, const E &expression) {
  const auto nodeCountBefore = ctx.node_count();
  if (!parse(expression, ctx)) {
    return false;
  }
  if (ctx.node_count() > nodeCountBefore) {
    ctx.override_grammar_element(nodeCountBefore, element);
  }
  return true;
}

} // namespace pegium::parser::detail
