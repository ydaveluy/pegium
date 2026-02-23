#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/Parser.hpp>

#include <cmath>
#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <variant>

using namespace pegium::parser;

namespace Arithmetics {

struct Module;
struct Expression : pegium::AstNode {};
struct AbstractDefinition : pegium::AstNode {};

struct DeclaredParameter : AbstractDefinition {
  string name;
};

struct Definition : AbstractDefinition {
  string name;
  vector<pointer<DeclaredParameter>> args;
  pointer<Expression> expr;
};

struct Evaluation : pegium::AstNode {
  pointer<Expression> expression;
};

struct Module : pegium::AstNode {
  string name;
  vector<pointer<pegium::AstNode>> statements;
};

struct BinaryExpression : Expression {
  pointer<Expression> left;
  string op;
  pointer<Expression> right;
};

struct NumberLiteral : Expression {
  double value = 0.0;
};

struct FunctionCall : Expression {
  reference<AbstractDefinition> func;
  vector<pointer<Expression>> args;
};

class ArithmeticParser : public Parser {
public:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<> WS{"WS", some(s)};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};

  Rule<Arithmetics::Module> Module{
      "Module",
      "module"_kw + assign<&Arithmetics::Module::name>(ID) +
          many(append<&Arithmetics::Module::statements>(Statement))};

  Rule<pegium::AstNode> Statement{"Statement", Definition | Evaluation};

  Rule<Arithmetics::Definition> Definition{
      "Definition",
      "def"_kw + assign<&Arithmetics::Definition::name>(ID) +
          option("("_kw +
                 append<&Arithmetics::Definition::args>(DeclaredParameter) +
                 many(","_kw +
                      append<&Arithmetics::Definition::args>(DeclaredParameter)) +
                 ")"_kw) +
          ":"_kw + assign<&Arithmetics::Definition::expr>(Expression) + ";"_kw};

  Rule<Arithmetics::DeclaredParameter> DeclaredParameter{
      "DeclaredParameter", assign<&Arithmetics::DeclaredParameter::name>(ID)};

  Rule<Arithmetics::Evaluation> Evaluation{
      "Evaluation",
      assign<&Arithmetics::Evaluation::expression>(Expression) + ";"_kw};

  Rule<Arithmetics::Expression> Expression{"Expression", Addition};


  Rule<Arithmetics::Expression> Addition{
      "Addition",
      Multiplication +
          many(action<&Arithmetics::BinaryExpression::left>() +
                   assign<&Arithmetics::BinaryExpression::op>("+"_kw | "-"_kw) +
                   assign<&Arithmetics::BinaryExpression::right>(
                       Multiplication))};


  Rule<Arithmetics::Expression> Multiplication{
      "Multiplication",
      Exponentiation +
          many(action<&Arithmetics::BinaryExpression::left>() +
                   assign<&Arithmetics::BinaryExpression::op>("*"_kw | "/"_kw ) +
                   assign<&Arithmetics::BinaryExpression::right>(
                       Exponentiation))};

  Rule<Arithmetics::Expression> Exponentiation{
      "Exponentiation",
      Modulo +
          many(action<&Arithmetics::BinaryExpression::left>() +
               assign<&Arithmetics::BinaryExpression::op>("^"_kw) +
               assign<&Arithmetics::BinaryExpression::right>(Modulo))};

  Rule<Arithmetics::Expression> Modulo{
      "Modulo",
      PrimaryExpression +
          many(action<&Arithmetics::BinaryExpression::left>() +
               assign<&Arithmetics::BinaryExpression::op>("%"_kw) +
               assign<&Arithmetics::BinaryExpression::right>(PrimaryExpression))};

  Rule<Arithmetics::Expression> PrimaryExpression{
      "PrimaryExpression",
      "("_kw + Expression + ")"_kw |
          action<Arithmetics::NumberLiteral>() +
              assign<&Arithmetics::NumberLiteral::value>(NUMBER) |
          action<Arithmetics::FunctionCall>() +
              assign<&Arithmetics::FunctionCall::func>(ID) +
              option("("_kw +
                     append<&Arithmetics::FunctionCall::args>(Expression) +
                     many(","_kw +
                          append<&Arithmetics::FunctionCall::args>(Expression)) +
                     ")"_kw)};
#pragma clang diagnostic pop


  auto createContext() const {
    return SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};

using ContextValue = std::variant<double, const Definition *>;
using EvaluationResult = std::unordered_map<const Evaluation *, double>;

struct InterpreterContext {
  Module *module = nullptr;
  std::unordered_map<std::string, ContextValue> context;
  EvaluationResult result;
};

static std::string getAbstractDefinitionName(const AbstractDefinition *def) {
  if (const auto *definition = dynamic_cast<const Definition *>(def)) {
    return definition->name;
  }
  if (const auto *parameter = dynamic_cast<const DeclaredParameter *>(def)) {
    return parameter->name;
  }
  throw std::runtime_error("Impossible type of AbstractDefinition.");
}

static pegium::AstNode *resolveAbstractDefinition(Module &module,
                                                  const std::string &name) {
  for (const auto &statement : module.statements) {
    const auto *definition = dynamic_cast<const Definition *>(statement.get());
    if (!definition) {
      continue;
    }

    if (definition->name == name) {
      return const_cast<Definition *>(definition);
    }

    for (const auto &arg : definition->args) {
      if (arg && arg->name == name) {
        return const_cast<DeclaredParameter *>(arg.get());
      }
    }
  }

  return nullptr;
}

static void installReferenceResolver(Module &module) {
  const auto resolver = [&module](const std::string &name) -> pegium::AstNode * {
    return resolveAbstractDefinition(module, name);
  };

  for (const auto &ref : module.getReferences()) {
    ref.installResolver(resolver);
  }
  for (auto *node : module.getAllContent()) {
    for (const auto &ref : node->getReferences()) {
      ref.installResolver(resolver);
    }
  }
}

static double applyOp(std::string_view op, double x, double y) {
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
      throw std::runtime_error("Division by zero");
    }
    return x / y;
  }
  throw std::runtime_error("Unknown operator: " + std::string(op));
}

static double evalExpression(const Expression &expr, InterpreterContext &ctx);

static void evalDefinition(InterpreterContext &ctx, const Definition &def) {
  if (def.args.empty()) {
    if (!def.expr) {
      throw std::runtime_error("Definition has no expression: " + def.name);
    }
    ctx.context[def.name] = evalExpression(*def.expr, ctx);
  } else {
    ctx.context[def.name] = &def;
  }
}

static void evalEvaluation(InterpreterContext &ctx, const Evaluation &evaluation) {
  if (!evaluation.expression) {
    throw std::runtime_error("Evaluation has no expression.");
  }
  ctx.result[&evaluation] = evalExpression(*evaluation.expression, ctx);
}

static void evalStatement(InterpreterContext &ctx, const pegium::AstNode &stmt) {
  if (const auto *definition = dynamic_cast<const Definition *>(&stmt)) {
    evalDefinition(ctx, *definition);
    return;
  }
  if (const auto *evaluation = dynamic_cast<const Evaluation *>(&stmt)) {
    evalEvaluation(ctx, *evaluation);
    return;
  }
  throw std::runtime_error("Impossible type of Statement.");
}

static EvaluationResult evaluate(InterpreterContext &ctx) {
  for (const auto &stmt : ctx.module->statements) {
    if (stmt) {
      evalStatement(ctx, *stmt);
    }
  }
  return ctx.result;
}

static double evalExpression(const Expression &expr, InterpreterContext &ctx) {
  if (const auto *binary = dynamic_cast<const BinaryExpression *>(&expr)) {
    if (!binary->left || !binary->right) {
      throw std::runtime_error("Invalid BinaryExpression.");
    }
    const auto left = evalExpression(*binary->left, ctx);
    const auto right = evalExpression(*binary->right, ctx);
    return applyOp(binary->op, left, right);
  }

  if (const auto *literal = dynamic_cast<const NumberLiteral *>(&expr)) {
    return literal->value;
  }

  if (const auto *call = dynamic_cast<const FunctionCall *>(&expr)) {
    const auto *ref = call->func.get();
    if (!ref) {
      throw std::runtime_error("Unknown reference in FunctionCall.");
    }

    const auto symbolName = getAbstractDefinitionName(ref);
    const auto it = ctx.context.find(symbolName);
    if (it == ctx.context.end()) {
      throw std::runtime_error("Unknown symbol: " + symbolName);
    }

    if (std::holds_alternative<double>(it->second)) {
      return std::get<double>(it->second);
    }

    const auto *definition = std::get<const Definition *>(it->second);
    if (!definition) {
      throw std::runtime_error("Invalid function definition: " + symbolName);
    }
    if (definition->args.size() != call->args.size()) {
      throw std::runtime_error(
          "Function definition and its call have different number of arguments: " +
          definition->name);
    }

    InterpreterContext localCtx{
        .module = ctx.module,
        .context = ctx.context,
        .result = ctx.result,
    };
    for (std::size_t i = 0; i < definition->args.size(); ++i) {
      if (!definition->args[i] || !call->args[i]) {
        throw std::runtime_error("Invalid function call arguments.");
      }
      localCtx.context[definition->args[i]->name] =
          evalExpression(*call->args[i], ctx);
    }

    if (!definition->expr) {
      throw std::runtime_error("Function has no expression: " + definition->name);
    }
    return evalExpression(*definition->expr, localCtx);
  }

  throw std::runtime_error("Impossible type of Expression.");
}

EvaluationResult interpretEvaluations(Module &module) {
  installReferenceResolver(module);
  InterpreterContext ctx{
      .module = &module,
      .context = {},
      .result = {},
  };
  return evaluate(ctx);
}

} // namespace Arithmetics

namespace {

const Arithmetics::BinaryExpression *asBinary(
    const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::BinaryExpression *>(expr);
}

const Arithmetics::FunctionCall *asFunctionCall(
    const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::FunctionCall *>(expr);
}

const Arithmetics::NumberLiteral *asNumber(const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::NumberLiteral *>(expr);
}

std::string makeArithmeticPayload(std::size_t repetitions) {
  std::string input = R"(
    module bench
    def square(x): x * x;
    def add(a, b): a + b;
    def mul(a, b): a * b;
    def constant: 10;
  )";

  for (std::size_t i = 0; i < repetitions; ++i) {
    input += "add(square(" + std::to_string(i % 97) + "), mul(" +
             std::to_string((i + 1) % 97) + ", " +
             std::to_string((i + 3) % 13 + 1) + ")) - constant;\n";
  }
  return input;
}

} // namespace

TEST(ArithmeticsTest, ParseModuleWithDefinitionsAndEvaluation) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module demo

    def square(x): x * x;
    def add(a, b): a + b;

    /* call both */
    add(1, 2) + square(3);
  )";

  auto result = parser.Module.parse(input, parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);

  EXPECT_EQ(result.value->name, "demo");
  ASSERT_EQ(result.value->statements.size(), 3u);

  const auto *squareDef =
      dynamic_cast<const Arithmetics::Definition *>(result.value->statements[0].get());
  ASSERT_TRUE(squareDef != nullptr);
  EXPECT_EQ(squareDef->name, "square");
  ASSERT_EQ(squareDef->args.size(), 1u);
  ASSERT_TRUE(squareDef->args[0] != nullptr);
  EXPECT_EQ(squareDef->args[0]->name, "x");

  const auto *evaluation =
      dynamic_cast<const Arithmetics::Evaluation *>(result.value->statements[2].get());
  ASSERT_TRUE(evaluation != nullptr);
  ASSERT_TRUE(evaluation->expression != nullptr);

  const auto *root = asBinary(evaluation->expression.get());
  ASSERT_TRUE(root != nullptr);
  EXPECT_EQ(root->op, "+");

  const auto *leftCall = asFunctionCall(root->left.get());
  const auto *rightCall = asFunctionCall(root->right.get());
  ASSERT_TRUE(leftCall != nullptr);
  ASSERT_TRUE(rightCall != nullptr);
  ASSERT_EQ(leftCall->args.size(), 2u);
  ASSERT_EQ(rightCall->args.size(), 1u);

  const auto *arg0 = asNumber(leftCall->args[0].get());
  const auto *arg1 = asNumber(leftCall->args[1].get());
  const auto *arg2 = asNumber(rightCall->args[0].get());
  ASSERT_TRUE(arg0 != nullptr);
  ASSERT_TRUE(arg1 != nullptr);
  ASSERT_TRUE(arg2 != nullptr);
  EXPECT_DOUBLE_EQ(arg0->value, 1.0);
  EXPECT_DOUBLE_EQ(arg1->value, 2.0);
  EXPECT_DOUBLE_EQ(arg2->value, 3.0);
}

TEST(ArithmeticsTest, HonorsOperatorPrecedenceAndAssociativity) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module m
    2 + 3 * 4 - 5;
  )";

  auto result = parser.Module.parse(input, parser.createContext());
  ASSERT_TRUE(result.ret);
  ASSERT_TRUE(result.value != nullptr);
  ASSERT_EQ(result.value->statements.size(), 1u);

  const auto *evaluation =
      dynamic_cast<const Arithmetics::Evaluation *>(result.value->statements[0].get());
  ASSERT_TRUE(evaluation != nullptr);
  ASSERT_TRUE(evaluation->expression != nullptr);

  const auto *minus = asBinary(evaluation->expression.get());
  ASSERT_TRUE(minus != nullptr);
  EXPECT_EQ(minus->op, "-");

  const auto *plus = asBinary(minus->left.get());
  ASSERT_TRUE(plus != nullptr);
  EXPECT_EQ(plus->op, "+");

  const auto *mul = asBinary(plus->right.get());
  ASSERT_TRUE(mul != nullptr);
  EXPECT_EQ(mul->op, "*");

  const auto *two = asNumber(plus->left.get());
  const auto *three = asNumber(mul->left.get());
  const auto *four = asNumber(mul->right.get());
  const auto *five = asNumber(minus->right.get());
  ASSERT_TRUE(two != nullptr);
  ASSERT_TRUE(three != nullptr);
  ASSERT_TRUE(four != nullptr);
  ASSERT_TRUE(five != nullptr);

  EXPECT_DOUBLE_EQ(two->value, 2.0);
  EXPECT_DOUBLE_EQ(three->value, 3.0);
  EXPECT_DOUBLE_EQ(four->value, 4.0);
  EXPECT_DOUBLE_EQ(five->value, 5.0);
}

TEST(ArithmeticsTest, SolverEvaluatesDefinitionsFunctionsAndExpressions) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    def square(x): x * x;
    def add(a, b): a + b;
    def constant: 10;

    square(3);
    add(square(2), constant);
    2 ^ 3 % 3;
    20 / 4 + constant;
  )";

  auto parsed = parser.Module.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.value != nullptr);

  std::vector<const Arithmetics::Evaluation *> evaluations;
  for (const auto &stmt : parsed.value->statements) {
    if (const auto *evaluation =
            dynamic_cast<const Arithmetics::Evaluation *>(stmt.get())) {
      evaluations.push_back(evaluation);
    }
  }
  ASSERT_EQ(evaluations.size(), 4u);

  const auto solved = Arithmetics::interpretEvaluations(*parsed.value);
  ASSERT_EQ(solved.size(), 4u);
  EXPECT_DOUBLE_EQ(solved.at(evaluations[0]), 9.0);
  EXPECT_DOUBLE_EQ(solved.at(evaluations[1]), 14.0);
  EXPECT_DOUBLE_EQ(solved.at(evaluations[2]), 1.0);
  EXPECT_DOUBLE_EQ(solved.at(evaluations[3]), 15.0);
}

TEST(ArithmeticsTest, SolverRejectsDivisionByZero) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    1 / 0;
  )";

  auto parsed = parser.Module.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.value != nullptr);

  try {
    (void)Arithmetics::interpretEvaluations(*parsed.value);
    FAIL() << "Expected division by zero to throw.";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("Division by zero"), std::string::npos);
  }
}

TEST(ArithmeticsTest, SolverRejectsMismatchedFunctionArity) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    def id(x): x;
    id(1, 2);
  )";

  auto parsed = parser.Module.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.value != nullptr);

  EXPECT_THROW((void)Arithmetics::interpretEvaluations(*parsed.value),
               std::runtime_error);
}

TEST(ArithmeticsBenchmark, ParseSpeedMicroBenchmark) {
  Arithmetics::ArithmeticParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_ARITHMETIC_REPETITIONS", 6'000, /*minValue*/ 1);
  const auto payload =
      makeArithmeticPayload(static_cast<std::size_t>(repetitions));

  const auto stats = pegium::test::runParseBenchmark(
      "arithmetic", payload, [&](std::string_view text) {
        return parser.Module.parse(text, parser.createContext());
      });
  pegium::test::assertMinThroughput("arithmetic", stats.mib_per_s);
}
