#include <arithmetics/core/Language.hpp>
#include <arithmetics/core/Operator.hpp>
#include <pegium/core/utils/TextUtils.hpp>

#include <algorithm>
#include <stdexcept>
#include <string>
#include <utility>

namespace arithmetics {

namespace {

using namespace ast;

std::string get_definition_name(const AbstractDefinition *definition) {
  if (const auto *typedDefinition = pegium::ast_ptr_cast<const Definition>(definition)) {
    return typedDefinition->name;
  }
  if (const auto *parameter =
          pegium::ast_ptr_cast<const DeclaredParameter>(definition)) {
    return parameter->name;
  }
  throw std::runtime_error("Unsupported abstract definition type.");
}

// Resolves an identifier to its definition or parameter by scanning the module.
// Used instead of the linked cross-reference because the interpreter also runs
// on parse-only (unlinked) documents (tests / recovery harnesses).
pegium::AstNode *resolve_definition(Module &module,
                                    const std::string &identifier) {
  for (auto &statement : module.statements) {
    auto *definition = pegium::ast_ptr_cast<Definition>(statement);
    if (!definition) {
      continue;
    }

    if (definition->name == identifier) {
      return definition;
    }

    for (auto &arg : definition->args) {
      if (arg && arg->name == identifier) {
        return arg;
      }
    }
  }
  return nullptr;
}

struct InterpreterContext {
  Module *module = nullptr;
  std::unordered_map<std::string, ContextValue> symbols;
  EvaluationResult results;
  std::vector<const Definition *> activeDefinitions;
};

double evaluate_expression(const Expression &expression, InterpreterContext &ctx);

void evaluate_evaluation(InterpreterContext &ctx, const Evaluation &evaluation) {
  if (!evaluation.expression) {
    throw std::runtime_error("Evaluation has no expression.");
  }
  ctx.results[&evaluation] = evaluate_expression(*evaluation.expression, ctx);
}

EvaluationResult evaluate_module_impl(InterpreterContext &ctx) {
  // Register every definition up front so forward references resolve; each is
  // evaluated on demand from the FunctionCall path when first referenced.
  for (const auto &statement : ctx.module->statements) {
    if (const auto *definition =
            pegium::ast_ptr_cast<const Definition>(statement)) {
      ctx.symbols[definition->name] = definition;
    }
  }
  for (const auto &statement : ctx.module->statements) {
    if (const auto *evaluation =
            pegium::ast_ptr_cast<const Evaluation>(statement)) {
      evaluate_evaluation(ctx, *evaluation);
    }
  }
  return ctx.results;
}

double evaluate_expression(const Expression &expression, InterpreterContext &ctx) {
  if (const auto *binary = pegium::ast_ptr_cast<const BinaryExpression>(&expression)) {
    if (!binary->left || !binary->right) {
      throw std::runtime_error("Invalid binary expression.");
    }
    const auto left = evaluate_expression(*binary->left, ctx);
    const auto right = evaluate_expression(*binary->right, ctx);
    return apply_operator(binary->op)(left, right);
  }

  if (const auto *grouped = pegium::ast_ptr_cast<const GroupedExpression>(&expression)) {
    if (!grouped->expression) {
      throw std::runtime_error("Grouped expression has no inner expression.");
    }
    return evaluate_expression(*grouped->expression, ctx);
  }

  if (const auto *literal = pegium::ast_ptr_cast<const NumberLiteral>(&expression)) {
    return literal->value;
  }

  if (const auto *call = pegium::ast_ptr_cast<const FunctionCall>(&expression)) {
    const auto *resolved = pegium::ast_ptr_cast<const AbstractDefinition>(
        resolve_definition(*ctx.module, canonical_identifier(call->func.getRefText())));
    if (!resolved) {
      throw std::runtime_error("Unknown reference in function call.");
    }

    const auto symbolName = get_definition_name(resolved);
    const auto symbol = ctx.symbols.find(symbolName);
    if (symbol == ctx.symbols.end()) {
      throw std::runtime_error("Unknown symbol: " + symbolName);
    }

    if (std::holds_alternative<double>(symbol->second)) {
      return std::get<double>(symbol->second);
    }

    const auto *definition = std::get<const Definition *>(symbol->second);
    if (!definition) {
      throw std::runtime_error("Invalid function definition: " + symbolName);
    }
    if (definition->args.size() != call->args.size()) {
      throw std::runtime_error(
          "Function arity mismatch for: " + definition->name);
    }
    if (std::ranges::find(ctx.activeDefinitions, definition) !=
        ctx.activeDefinitions.end()) {
      throw std::runtime_error("Recursive function call detected: " +
                               definition->name);
    }

    InterpreterContext localCtx = ctx;
    localCtx.activeDefinitions.push_back(definition);

    for (std::size_t i = 0; i < definition->args.size(); ++i) {
      if (!definition->args[i] || !call->args[i]) {
        throw std::runtime_error("Invalid function call arguments.");
      }
      localCtx.symbols[definition->args[i]->name] =
          evaluate_expression(*call->args[i], ctx);
    }

    if (!definition->expr) {
      throw std::runtime_error("Function has no body: " + definition->name);
    }
    return evaluate_expression(*definition->expr, localCtx);
  }

  throw std::runtime_error("Unsupported expression type.");
}

} // namespace

std::string canonical_identifier(std::string_view text) {
  std::string value;
  value.reserve(text.size());
  // Identifiers are ASCII ([A-Za-z0-9_]); pegium::utils::tolower is a branch-free
  // 256-byte table fold, vs std::tolower's per-byte locale-facet indirection.
  for (const char c : text) {
    value.push_back(pegium::utils::tolower(c));
  }
  return value;
}

EvaluationResult interpret_evaluations(ast::Module &module) {
  InterpreterContext ctx{
      .module = &module,
      .symbols = {},
      .results = {},
      .activeDefinitions = {},
  };
  return evaluate_module_impl(ctx);
}

std::vector<double> evaluate_module(ast::Module &module) {
  const auto results = interpret_evaluations(module);

  std::vector<double> orderedResults;
  for (const auto &statement : module.statements) {
    if (const auto *evaluation =
            pegium::ast_ptr_cast<const ast::Evaluation>(statement)) {
      orderedResults.push_back(results.at(evaluation));
    }
  }
  return orderedResults;
}

} // namespace arithmetics
