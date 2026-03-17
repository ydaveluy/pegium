#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>

#include <cmath>
#include <functional>
#include <optional>
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

class ArithmeticParser : public PegiumParser {
public:
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Module;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper =
      SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();

  Terminal<> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};

  Rule<Arithmetics::Module> Module{
      "Module", "module"_kw + assign<&Arithmetics::Module::name>(ID) +
                    many(append<&Arithmetics::Module::statements>(Statement))};

  Rule<pegium::AstNode> Statement{"Statement", Definition | Evaluation};

  Rule<Arithmetics::Definition> Definition{
      "Definition",
      "def"_kw + assign<&Arithmetics::Definition::name>(ID) +
          option("("_kw +
                 append<&Arithmetics::Definition::args>(DeclaredParameter) +
                 many(","_kw + append<&Arithmetics::Definition::args>(
                                   DeclaredParameter)) +
                 ")"_kw) +
          ":"_kw + assign<&Arithmetics::Definition::expr>(Expression) + ";"_kw};

  Rule<Arithmetics::DeclaredParameter> DeclaredParameter{
      "DeclaredParameter", assign<&Arithmetics::DeclaredParameter::name>(ID)};

  Rule<Arithmetics::Evaluation> Evaluation{
      "Evaluation",
      assign<&Arithmetics::Evaluation::expression>(Expression) + ";"_kw};

  Rule<Arithmetics::Expression> Expression{"Expression", BinaryExpression};

  InfixRule<Arithmetics::BinaryExpression, &Arithmetics::BinaryExpression::left,
            &Arithmetics::BinaryExpression::op,
            &Arithmetics::BinaryExpression::right>
      BinaryExpression{"BinaryExpression",
                       PrimaryExpression,
                       LeftAssociation("%"_kw),
                       LeftAssociation("^"_kw),
                       LeftAssociation("*"_kw | "/"_kw),
                       LeftAssociation("+"_kw | "-"_kw)};

  Rule<Arithmetics::Expression> Addition{
      "Addition",
      Multiplication +
          many(nest<&Arithmetics::BinaryExpression::left>() +
               assign<&Arithmetics::BinaryExpression::op>("+"_kw | "-"_kw) +
               assign<&Arithmetics::BinaryExpression::right>(Multiplication))};

  Rule<Arithmetics::Expression> Multiplication{
      "Multiplication",
      Exponentiation +
          many(nest<&Arithmetics::BinaryExpression::left>() +
               assign<&Arithmetics::BinaryExpression::op>("*"_kw | "/"_kw) +
               assign<&Arithmetics::BinaryExpression::right>(Exponentiation))};

  Rule<Arithmetics::Expression> Exponentiation{
      "Exponentiation",
      Modulo + many(nest<&Arithmetics::BinaryExpression::left>() +
                    assign<&Arithmetics::BinaryExpression::op>("^"_kw) +
                    assign<&Arithmetics::BinaryExpression::right>(Modulo))};

  Rule<Arithmetics::Expression> Modulo{
      "Modulo", PrimaryExpression +
                    many(nest<&Arithmetics::BinaryExpression::left>() +
                         assign<&Arithmetics::BinaryExpression::op>("%"_kw) +
                         assign<&Arithmetics::BinaryExpression::right>(
                             PrimaryExpression))};

  Rule<Arithmetics::Expression> PrimaryExpression{
      "PrimaryExpression",
      "("_kw + Expression + ")"_kw |
          create<Arithmetics::NumberLiteral>() +
              assign<&Arithmetics::NumberLiteral::value>(NUMBER) |
          create<Arithmetics::FunctionCall>() +
              assign<&Arithmetics::FunctionCall::func>(ID) +
              option("("_kw +
                     append<&Arithmetics::FunctionCall::args>(Expression) +
                     many(","_kw + append<&Arithmetics::FunctionCall::args>(
                                       Expression)) +
                     ")"_kw)};
#pragma clang diagnostic pop
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
  for (auto *call : module.getAllContent<FunctionCall>()) {
    auto &reference = call->func;
    if (auto *target = resolveAbstractDefinition(module, reference.getRefText());
        target != nullptr) {
      reference.setResolution(
          pegium::ReferenceResolution{.node = target, .description = nullptr});
      continue;
    }
    reference.setResolution(pegium::ReferenceResolution{
        .node = nullptr,
        .description = nullptr,
        .errorMessage = "Unknown symbol: " + std::string(reference.getRefText()),
    });
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

static void evalEvaluation(InterpreterContext &ctx,
                           const Evaluation &evaluation) {
  if (!evaluation.expression) {
    throw std::runtime_error("Evaluation has no expression.");
  }
  ctx.result[&evaluation] = evalExpression(*evaluation.expression, ctx);
}

static void evalStatement(InterpreterContext &ctx,
                          const pegium::AstNode &stmt) {
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
      throw std::runtime_error("Function definition and its call have "
                               "different number of arguments: " +
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
      throw std::runtime_error("Function has no expression: " +
                               definition->name);
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

const Arithmetics::BinaryExpression *
asBinary(const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::BinaryExpression *>(expr);
}

const Arithmetics::FunctionCall *
asFunctionCall(const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::FunctionCall *>(expr);
}

const Arithmetics::NumberLiteral *
asNumber(const Arithmetics::Expression *expr) {
  return dynamic_cast<const Arithmetics::NumberLiteral *>(expr);
}

std::optional<pegium::CstNodeView>
find_smallest_visible_node_containing(const pegium::CstNodeView &node,
                                      pegium::grammar::ElementKind kind,
                                      std::string_view fragment) {
  std::optional<pegium::CstNodeView> best;
  if (!node.isHidden() && node.getGrammarElement()->getKind() == kind &&
      node.getText().find(fragment) != std::string_view::npos) {
    best = node;
  }
  for (const auto child : node) {
    if (auto found =
            find_smallest_visible_node_containing(child, kind, fragment);
        found.has_value()) {
      if (!best.has_value() || (found->getEnd() - found->getBegin() <
                                best->getEnd() - best->getBegin())) {
        best = found;
      }
    }
  }
  return best;
}

std::vector<pegium::CstNodeView>
visible_children_by_kind(const pegium::CstNodeView &node,
                         pegium::grammar::ElementKind kind) {
  std::vector<pegium::CstNodeView> children;
  for (const auto child : node) {
    if (child.isHidden()) {
      continue;
    }
    if (child.getGrammarElement()->getKind() == kind) {
      children.push_back(child);
    }
  }
  return children;
}

void collect_visible_nodes_by_kind(const pegium::CstNodeView &node,
                                   pegium::grammar::ElementKind kind,
                                   std::vector<pegium::CstNodeView> &out) {
  if (!node.isHidden() && node.getGrammarElement()->getKind() == kind) {
    out.push_back(node);
  }
  for (const auto child : node) {
    collect_visible_nodes_by_kind(child, kind, out);
  }
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

std::string makeLateRecoveryTailPayload(std::size_t repetitions) {
  std::string input = "module calc\n";
  for (std::size_t i = 0; i < repetitions; ++i) {
    (void)i;
    input += "2*4+7-7/1*9-711*69*22;\n";
  }
  input += "2*4+7-7/1*9-711*69*22;v\n";
  input += "2/0;\n";
  return input;
}

std::string makeRecoveryCrossLinePrimaryPayload() {
  return "module calc\n"
         "2*4+7-7/1*9-711*69*22;v\n"
         "2*4+7-7/1*9-711*69*22;\n"
         "2*4+7-7/1*9-711*69*22;\n"
         "2*4+7-7/1*9-711*69*22;\n"
         "2*4+7-7/1*9-711*69*22;\n"
         "2*4+7-7/1*9-711*69*22/0;\n";
}

template <typename ParserType>
auto parse_text(const ParserType &parser, std::string_view text,
                const ParseOptions &options = {}) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  parser.parse(*document);
  (void)options;
  return document;
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

  auto document = parse_text(parser, input);
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(result.value);
  ASSERT_TRUE(module != nullptr);

  EXPECT_EQ(module->name, "demo");
  ASSERT_EQ(module->statements.size(), 3u);

  const auto *squareDef = dynamic_cast<const Arithmetics::Definition *>(
      module->statements[0].get());
  ASSERT_TRUE(squareDef != nullptr);
  EXPECT_EQ(squareDef->name, "square");
  ASSERT_EQ(squareDef->args.size(), 1u);
  ASSERT_TRUE(squareDef->args[0] != nullptr);
  EXPECT_EQ(squareDef->args[0]->name, "x");

  const auto *evaluation = dynamic_cast<const Arithmetics::Evaluation *>(
      module->statements[2].get());
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

  auto document = parse_text(parser, input);
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(result.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_EQ(module->statements.size(), 1u);

  const auto *evaluation = dynamic_cast<const Arithmetics::Evaluation *>(
      module->statements[0].get());
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

TEST(ArithmeticsTest, InfixRuleBuildsCascadeCstForLinearChain) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module m
    1+2+3+4+5;
  )";

  auto document = parse_text(parser, input);
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  ASSERT_TRUE(result.cst != nullptr);

  std::optional<pegium::CstNodeView> expressionNode;
  for (const auto top : *result.cst) {
    expressionNode = find_smallest_visible_node_containing(
        top, pegium::grammar::ElementKind::ParserRule, "1+2+3+4+5");
    if (expressionNode.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(expressionNode.has_value());

  std::vector<pegium::CstNodeView> infixNodes;
  collect_visible_nodes_by_kind(
      *expressionNode, pegium::grammar::ElementKind::InfixRule, infixNodes);
  ASSERT_EQ(infixNodes.size(), 4u);
  EXPECT_EQ(infixNodes[0].getText(), "1+2");
  EXPECT_EQ(infixNodes[1].getText(), "1+2+3");
  EXPECT_EQ(infixNodes[2].getText(), "1+2+3+4");
  EXPECT_EQ(infixNodes[3].getText(), "1+2+3+4+5");
}

TEST(ArithmeticsTest, InfixRuleBuildsCascadeCstForMixedPrecedence) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module m
    1+2*3-4/5;
  )";

  auto document = parse_text(parser, input);
  auto &result = document->parseResult;
  ASSERT_TRUE(result.value);
  ASSERT_TRUE(result.cst != nullptr);

  std::optional<pegium::CstNodeView> expressionNode;
  for (const auto top : *result.cst) {
    expressionNode = find_smallest_visible_node_containing(
        top, pegium::grammar::ElementKind::ParserRule, "1+2*3-4/5");
    if (expressionNode.has_value()) {
      break;
    }
  }
  ASSERT_TRUE(expressionNode.has_value());

  std::vector<pegium::CstNodeView> infixNodes;
  collect_visible_nodes_by_kind(
      *expressionNode, pegium::grammar::ElementKind::InfixRule, infixNodes);
  ASSERT_EQ(infixNodes.size(), 4u);
  EXPECT_EQ(infixNodes[0].getText(), "1+2*3");
  EXPECT_EQ(infixNodes[1].getText(), "2*3");
  EXPECT_EQ(infixNodes[2].getText(), "1+2*3-4/5");
  EXPECT_EQ(infixNodes[3].getText(), "4/5");

  const auto leftChildren = visible_children_by_kind(
      infixNodes[0], pegium::grammar::ElementKind::InfixRule);
  ASSERT_EQ(leftChildren.size(), 1u);
  EXPECT_EQ(leftChildren.front().getText(), "2*3");

  const auto rightChildren = visible_children_by_kind(
      infixNodes[2], pegium::grammar::ElementKind::InfixRule);
  ASSERT_EQ(rightChildren.size(), 1u);
  EXPECT_EQ(rightChildren.front().getText(), "4/5");
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

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);

  std::vector<const Arithmetics::Evaluation *> evaluations;
  for (const auto &stmt : module->statements) {
    if (const auto *evaluation =
            dynamic_cast<const Arithmetics::Evaluation *>(stmt.get())) {
      evaluations.push_back(evaluation);
    }
  }
  ASSERT_EQ(evaluations.size(), 4u);

  const auto solved = Arithmetics::interpretEvaluations(*module);
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

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);

  try {
    (void)Arithmetics::interpretEvaluations(*module);
    FAIL() << "Expected division by zero to throw.";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("Division by zero"),
              std::string::npos);
  }
}

TEST(ArithmeticsTest, SolverRejectsMismatchedFunctionArity) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    def id(x): x;
    id(1, 2);
  )";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);

  EXPECT_THROW((void)Arithmetics::interpretEvaluations(*module),
               std::runtime_error);
}

TEST(ArithmeticsTest, RecoveryHandlesDanglingPlusBeforeSemicolon) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    def a: 5;
    def b: 3;
    def c: a + b;
    2 * c+; // 16
    b % 2; // 1
    2 * c; // 16
    b % 2; // 1
    2 * c; // 16
    b % 2; // 1
  )";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  const auto danglingPlusOffset = input.find("c+;");
  const auto nextStatementOffset = input.find("\n    b % 2; // 1");
  ASSERT_NE(danglingPlusOffset, std::string::npos);
  ASSERT_NE(nextStatementOffset, std::string::npos);
  for (const auto &diag : parsed.parseDiagnostics) {
    EXPECT_LT(diag.offset, nextStatementOffset)
        << "Recovery edited valid trailing statements.";
  }

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 9u);

  const auto recoveredEvaluationCount = std::count_if(
      module->statements.begin(), module->statements.end(), [](const auto &statement) {
        const auto *evaluation =
            dynamic_cast<const Arithmetics::Evaluation *>(statement.get());
        return evaluation != nullptr && evaluation->expression != nullptr;
      });
  EXPECT_GE(recoveredEvaluationCount, 5);
}

TEST(ArithmeticsTest,
     RecoveryHandlesDanglingPlusWithExtraSpacesBeforeSemicolon) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module calc
    def a: 5;
    def b: 3;
    def c: a + b;
    2 * c     +   ; // 16
    b % 2; // 1
    2 * c; // 16
    b % 2; // 1
    2 * c; // 16
    b % 2; // 1
  )";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 9u);

  const auto recoveredEvaluationCount = std::count_if(
      module->statements.begin(), module->statements.end(), [](const auto &statement) {
        const auto *evaluation =
            dynamic_cast<const Arithmetics::Evaluation *>(statement.get());
        return evaluation != nullptr && evaluation->expression != nullptr;
      });
  EXPECT_GE(recoveredEvaluationCount, 5);
}

TEST(ArithmeticsTest, RecoveryReportsMissingOperatorAndKeepsDivisionByZero) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module basicMath
    def c: 8;
    5 2 * c/0  + 1 -3  ;
  )";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  const auto malformedOffset = input.find("5 2");
  ASSERT_NE(malformedOffset, std::string::npos);
  bool hasMalformedDiagnostic = false;
  for (const auto &diag : parsed.parseDiagnostics) {
    if (diag.offset >= malformedOffset && diag.offset <= malformedOffset + 2) {
      hasMalformedDiagnostic = true;
      break;
    }
  }
  EXPECT_TRUE(hasMalformedDiagnostic)
      << "Expected a recovery diagnostic around malformed token sequence '5 "
         "2'.";

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 2u);

  try {
    (void)Arithmetics::interpretEvaluations(*module);
    FAIL() << "Expected division by zero to throw.";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("Division by zero"),
              std::string::npos);
  }
}

TEST(ArithmeticsTest, RecoveryRepairsMissingCodepointForDefKeyword) {
  Arithmetics::ArithmeticParser parser;

  const std::string input =
      "module basicMath\n"
      "\n"
      "de a: 5;\n";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(
      parsed.parseDiagnostics, [](const auto &diagnostic) {
        return diagnostic.kind == pegium::parser::ParseDiagnosticKind::Replaced;
      }));

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_EQ(module->statements.size(), 1u);
  auto *definition =
      dynamic_cast<const Arithmetics::Definition *>(module->statements[0].get());
  ASSERT_TRUE(definition != nullptr);
  EXPECT_EQ(definition->name, "a");
}

TEST(ArithmeticsTest, RecoveryContinuesAfterTrailingStatementFragment) {
  Arithmetics::ArithmeticParser parser;

  const std::string input =
      "module basicMath\n"
      "\n"
      "def a: 5;\n"
      "def b: 3;\n"
      "def c: a + b; // 8\n"
      "def d: (a ^ b); // 125\n"
      "\n"
      "def root(x, y):\n"
      "    x^(1/y);\n"
      "\n"
      "def sqrt(x):\n"
      "    root(x, 2);\n"
      "\n"
      "2 * c; // 16\n"
      "b % 2; // 1\n"
      "\n"
      "root(d, 3); // 5\n"
      "root(64, 3); // 4\n"
      "sqrt(81); // 9\n"
      "\n"
      "2*4+7-7/1*9-711*69*22;v\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22/0;\n";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 12u);

  try {
    (void)Arithmetics::interpretEvaluations(*module);
    FAIL() << "Expected division by zero to throw.";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("Division by zero"),
              std::string::npos);
  }
}

TEST(ArithmeticsTest, RecoveryKeepsDefinitionWhenSemicolonIsMissingAtEof) {
  Arithmetics::ArithmeticParser parser;

  std::string input = R"(
    module id

    def a : 3  +5
  )";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  ASSERT_FALSE(parsed.parseDiagnostics.empty());
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics,
                                  [](const auto &diagnostic) {
                                    return diagnostic.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_EQ(module->statements.size(), 1u);

  auto *definition =
      dynamic_cast<Arithmetics::Definition *>(module->statements.front().get());
  ASSERT_NE(definition, nullptr);
  EXPECT_EQ(definition->name, "a");

  auto *binary =
      dynamic_cast<Arithmetics::BinaryExpression *>(definition->expr.get());
  ASSERT_NE(binary, nullptr);
  EXPECT_EQ(binary->op, "+");

  auto *left =
      dynamic_cast<Arithmetics::NumberLiteral *>(binary->left.get());
  auto *right =
      dynamic_cast<Arithmetics::NumberLiteral *>(binary->right.get());
  ASSERT_NE(left, nullptr);
  ASSERT_NE(right, nullptr);
  EXPECT_DOUBLE_EQ(left->value, 3.0);
  EXPECT_DOUBLE_EQ(right->value, 5.0);
}

TEST(ArithmeticsTest, RecoveryPrefersInsertedSemicolonBeforeNextDefinition) {
  Arithmetics::ArithmeticParser parser;

  const std::string input =
      "module basicMath\n"
      "\n"
      "def a: 5\n"
      "def b: 3;\n";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics,
                                  [](const auto &diag) {
                                    return diag.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));
  EXPECT_FALSE(std::ranges::any_of(parsed.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Deleted;
                                   }));

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_EQ(module->statements.size(), 2u);

  const auto *firstDefinition = dynamic_cast<const Arithmetics::Definition *>(
      module->statements[0].get());
  const auto *secondDefinition = dynamic_cast<const Arithmetics::Definition *>(
      module->statements[1].get());
  ASSERT_NE(firstDefinition, nullptr);
  ASSERT_NE(secondDefinition, nullptr);
  EXPECT_EQ(firstDefinition->name, "a");
  EXPECT_EQ(secondDefinition->name, "b");
}

TEST(ArithmeticsTest, RecoverySplitsWordBoundaryAfterModuleKeyword) {
  Arithmetics::ArithmeticParser parser;

  auto document = parse_text(parser, "modulebasicMath\n");
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics,
                                  [](const auto &diag) {
                                    return diag.kind ==
                                           ParseDiagnosticKind::Inserted;
                                  }));
  EXPECT_FALSE(std::ranges::any_of(parsed.parseDiagnostics,
                                   [](const auto &diag) {
                                     return diag.kind ==
                                            ParseDiagnosticKind::Deleted;
                                   }));

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  EXPECT_EQ(module->name, "basicMath");
}

TEST(ArithmeticsTest, RecoveryRepairsMissingKeywordCodepointForModule) {
  Arithmetics::ArithmeticParser parser;

  auto document = parse_text(parser, "modle basicMath\n");
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto replaced =
      std::ranges::find_if(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, parsed.parseDiagnostics.end());
  EXPECT_EQ(replaced->offset, 0u);
  EXPECT_NE(replaced->element, nullptr);

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  EXPECT_EQ(module->name, "basicMath");
}

TEST(ArithmeticsTest, RecoveryRepairsMissingKeywordSuffixForModule) {
  Arithmetics::ArithmeticParser parser;

  auto document = parse_text(parser, "Mod basicMath\n");
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);

  const auto replaced =
      std::ranges::find_if(parsed.parseDiagnostics, [](const auto &diag) {
        return diag.kind == ParseDiagnosticKind::Replaced;
      });
  ASSERT_NE(replaced, parsed.parseDiagnostics.end());
  EXPECT_EQ(replaced->offset, 0u);

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  EXPECT_EQ(module->name, "basicMath");
}

TEST(ArithmeticsTest, RecoveryDeletesUnexpectedTokenAfterOperator) {
  Arithmetics::ArithmeticParser parser;

  const std::string input =
      "module basicMath\n"
      "\n"
      "def c: 8;\n"
      "\n"
      "2 * +c;\n";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics,
                                  [](const auto &diag) {
                                    return diag.kind ==
                                           ParseDiagnosticKind::Deleted;
                                  }));

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);

  const auto results = Arithmetics::interpretEvaluations(*module);
  ASSERT_EQ(results.size(), 1u);
  EXPECT_DOUBLE_EQ(results.begin()->second, 16.0);
}

TEST(ArithmeticsTest, LateRecoveryTailStopsAfterSingleInertWindow) {
  Arithmetics::ArithmeticParser parser;

  const auto input = makeLateRecoveryTailPayload(256);
  auto document = parse_text(parser, input);
  const auto &parsed = document->parseResult;

  EXPECT_FALSE(parsed.fullMatch);
  EXPECT_FALSE(parsed.recoveryReport.hasRecovered);
  EXPECT_EQ(parsed.recoveryReport.recoveryCount, 0u);
  EXPECT_EQ(parsed.recoveryReport.recoveryWindowsTried, 1u);
  EXPECT_LE(parsed.recoveryReport.recoveryAttemptRuns, 1u);
  EXPECT_LT(parsed.parsedLength, input.size());
}

TEST(ArithmeticsTest,
     RecoveryDoesNotMergeUnexpectedPrimaryAcrossLineBreak) {
  Arithmetics::ArithmeticParser parser;

  const auto input = makeRecoveryCrossLinePrimaryPayload();
  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  ASSERT_FALSE(parsed.parseDiagnostics.empty());

  const auto unexpectedOffset = input.find(";v\n");
  ASSERT_NE(unexpectedOffset, std::string::npos);
  const auto deletedOffset = static_cast<pegium::TextOffset>(unexpectedOffset + 1);
  const auto nextLineOffset = deletedOffset + 2;

  EXPECT_TRUE(std::ranges::none_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == ParseDiagnosticKind::Deleted &&
           diag.offset == nextLineOffset;
  })) << "Recovery deleted the beginning of the next line instead of the "
          "unexpected token.";

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 6u);
}

TEST(ArithmeticsTest,
     RecoveryDeletesLeadingUnexpectedPrimaryAndKeepsDivisionByZero) {
  Arithmetics::ArithmeticParser parser;

  const std::string input =
      "module basicMath\n"
      "def c: 8;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "4 2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22;\n"
      "2*4+7-7/1*9-711*69*22/0;\n";

  auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;

  ASSERT_TRUE(parsed.value);
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_TRUE(parsed.recoveryReport.hasRecovered);
  ASSERT_FALSE(parsed.parseDiagnostics.empty());

  const auto statementOffset = input.find("\n4 2*4+7");
  ASSERT_NE(statementOffset, std::string::npos);
  const auto deletedPrimaryOffset =
      static_cast<pegium::TextOffset>(statementOffset + 1);

  EXPECT_TRUE(std::ranges::any_of(parsed.parseDiagnostics, [&](const auto &diag) {
    return diag.kind == ParseDiagnosticKind::Deleted &&
           diag.offset == deletedPrimaryOffset;
  })) << "Expected recovery to delete the leading unexpected primary token "
         "instead of inventing an operator.";

  auto *module = pegium::ast_ptr_cast<Arithmetics::Module>(parsed.value);
  ASSERT_TRUE(module != nullptr);
  ASSERT_GE(module->statements.size(), 6u);

  try {
    (void)Arithmetics::interpretEvaluations(*module);
    FAIL() << "Expected division by zero to throw.";
  } catch (const std::runtime_error &e) {
    EXPECT_NE(std::string(e.what()).find("Division by zero"),
              std::string::npos);
  }
}

TEST(ArithmeticsBenchmark, ParseSpeedMicroBenchmark) {
  Arithmetics::ArithmeticParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_ARITHMETIC_REPETITIONS", 6'000, /*minValue*/ 1);
  const auto payload =
      makeArithmeticPayload(static_cast<std::size_t>(repetitions));

  const auto stats = pegium::test::runParseBenchmark(
      "arithmetic", payload,
      [&](std::string_view text) { return parse_text(parser, text); });
  pegium::test::assertMinThroughput("arithmetic", stats.mib_per_s);
}
