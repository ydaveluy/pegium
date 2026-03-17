#include <arithmetics/parser/Parser.hpp>

#include <algorithm>
#include <cmath>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace arithmetics {

namespace {

using namespace ast;

std::string get_definition_name(const AbstractDefinition *definition) {
  if (const auto *typedDefinition = dynamic_cast<const Definition *>(definition)) {
    return typedDefinition->name;
  }
  if (const auto *parameter =
          dynamic_cast<const DeclaredParameter *>(definition)) {
    return parameter->name;
  }
  throw std::runtime_error("Unsupported abstract definition type.");
}

pegium::AstNode *resolve_definition(Module &module,
                                    const std::string &identifier) {
  for (const auto &statement : module.statements) {
    const auto *definition = dynamic_cast<const Definition *>(statement.get());
    if (!definition) {
      continue;
    }

    if (definition->name == identifier) {
      return const_cast<Definition *>(definition);
    }

    for (const auto &arg : definition->args) {
      if (arg && arg->name == identifier) {
        return const_cast<DeclaredParameter *>(arg.get());
      }
    }
  }
  return nullptr;
}

void resolve_references(Module &module) {
  for (auto *node : module.getAllContent()) {
    auto *call = dynamic_cast<FunctionCall *>(node);
    if (call == nullptr) {
      continue;
    }

    if (auto *target =
            resolve_definition(module, canonical_identifier(call->func.getRefText()));
        target != nullptr) {
      call->func.setResolution(
          pegium::ReferenceResolution{.node = target, .description = nullptr});
      continue;
    }

    call->func.setResolution(pegium::ReferenceResolution{
        .node = nullptr,
        .description = nullptr,
        .errorMessage = "Unknown reference in function call.",
    });
  }
}

double apply_operator(std::string_view op, double x, double y) {
  if (op == "+") {
    return x + y;
  }
  if (op == "-") {
    return x - y;
  }
  if (op == "*") {
    return x * y;
  }
  if (op == "^") {
    return std::pow(x, y);
  }
  if (op == "%") {
    return std::fmod(x, y);
  }
  if (op == "/") {
    if (y == 0.0) {
      throw std::runtime_error("Division by zero.");
    }
    return x / y;
  }
  throw std::runtime_error("Unknown operator: " + std::string(op));
}

struct InterpreterContext {
  Module *module = nullptr;
  std::unordered_map<std::string, ContextValue> symbols;
  EvaluationResult results;
  std::vector<const Definition *> activeDefinitions;
};

double evaluate_expression(const Expression &expression, InterpreterContext &ctx);

void evaluate_definition(InterpreterContext &ctx, const Definition &definition) {
  if (definition.args.empty()) {
    if (!definition.expr) {
      throw std::runtime_error("Definition has no expression: " +
                               definition.name);
    }
    ctx.symbols[definition.name] = evaluate_expression(*definition.expr, ctx);
  } else {
    ctx.symbols[definition.name] = &definition;
  }
}

void evaluate_evaluation(InterpreterContext &ctx, const Evaluation &evaluation) {
  if (!evaluation.expression) {
    throw std::runtime_error("Evaluation has no expression.");
  }
  ctx.results[&evaluation] = evaluate_expression(*evaluation.expression, ctx);
}

void evaluate_statement(InterpreterContext &ctx,
                        const pegium::AstNode &statement) {
  if (const auto *definition = dynamic_cast<const Definition *>(&statement)) {
    evaluate_definition(ctx, *definition);
    return;
  }
  if (const auto *evaluation = dynamic_cast<const Evaluation *>(&statement)) {
    evaluate_evaluation(ctx, *evaluation);
    return;
  }
  throw std::runtime_error("Unsupported statement type.");
}

EvaluationResult evaluate_module_impl(InterpreterContext &ctx) {
  for (const auto &statement : ctx.module->statements) {
    if (statement) {
      evaluate_statement(ctx, *statement);
    }
  }
  return ctx.results;
}

double evaluate_expression(const Expression &expression, InterpreterContext &ctx) {
  if (const auto *binary = dynamic_cast<const BinaryExpression *>(&expression)) {
    if (!binary->left || !binary->right) {
      throw std::runtime_error("Invalid binary expression.");
    }
    const auto left = evaluate_expression(*binary->left, ctx);
    const auto right = evaluate_expression(*binary->right, ctx);
    return apply_operator(binary->op, left, right);
  }

  if (const auto *grouped = dynamic_cast<const GroupedExpression *>(&expression)) {
    if (!grouped->expression) {
      throw std::runtime_error("Grouped expression has no inner expression.");
    }
    return evaluate_expression(*grouped->expression, ctx);
  }

  if (const auto *literal = dynamic_cast<const NumberLiteral *>(&expression)) {
    return literal->value;
  }

  if (const auto *call = dynamic_cast<const FunctionCall *>(&expression)) {
    const auto *resolved = call->func.get();
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
  for (const unsigned char c : text) {
    value.push_back(static_cast<char>(std::tolower(c)));
  }
  return value;
}

EvaluationResult interpret_evaluations(ast::Module &module) {
  resolve_references(module);
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
            dynamic_cast<const ast::Evaluation *>(statement.get())) {
      orderedResults.push_back(results.at(evaluation));
    }
  }
  return orderedResults;
}

} // namespace arithmetics

namespace arithmetics::parser {

std::unique_ptr<pegium::services::Services>
make_language_services(const pegium::services::SharedServices &sharedServices,
                       std::string languageId) {
  auto services =
      pegium::services::makeDefaultServices(sharedServices, std::move(languageId));
  services->parser = std::make_unique<const ArithmeticParser>(*services);
  return services;
}

} // namespace arithmetics::parser
