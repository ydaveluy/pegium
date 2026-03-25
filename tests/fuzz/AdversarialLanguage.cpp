#include <fuzz/AdversarialLanguage.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace pegium::fuzz::adversarial {
namespace ast {

struct Statement;
struct Node;
struct Slot;
struct Alias;
struct Link;
struct Mix;
struct Probe;
struct Pack;
struct CaseBlock;
struct Expression;
struct MapEntry;
struct NumberExpression;
struct BooleanExpression;
struct StringExpression;
struct ReferenceExpression;
struct CallExpression;
struct ListExpression;
struct MapExpression;
struct GroupedExpression;
struct BinaryExpression;
struct EvalStatement;
struct LegacyEvalStatement;

struct Model : pegium::AstNode {
  string name;
  vector<pointer<Statement>> statements;
};

struct Statement : pegium::AstNode {};

struct Node : Statement {
  bool exported = false;
  string name;
  optional<reference<Node>> superType;
  pointer<Expression> guard;
  vector<pointer<Slot>> slots;
};

struct Slot : pegium::AstNode {
  bool many = false;
  string name;
  reference<Node> type;
  pointer<Expression> fallback;
};

struct Alias : Statement {
  string name;
  reference<Node> target;
};

struct Link : Statement {
  reference<Node> source;
  reference<Node> target;
  pointer<Expression> guard;
};

struct Mix : Statement {
  bool hot = false;
  bool cold = false;
  bool warm = false;
  bool dry = false;
};

struct Probe : Statement {
  string mode;
};

struct Pack : Statement {
  vector<string> items;
};

struct CaseBlock : Statement {
  string name;
  pointer<Expression> guard;
  vector<pointer<Statement>> statements;
};

struct Expression : pegium::AstNode {};

struct NumberExpression : Expression {
  double value = 0.0;
};

struct BooleanExpression : Expression {
  bool value = false;
};

struct StringExpression : Expression {
  string value;
};

struct ReferenceExpression : Expression {
  reference<Node> target;
};

struct CallExpression : Expression {
  reference<Node> callee;
  vector<reference<Node>> typeArgs;
  vector<pointer<Expression>> args;
};

struct ListExpression : Expression {
  vector<pointer<Expression>> items;
};

struct MapEntry : pegium::AstNode {
  string key;
  pointer<Expression> value;
};

struct MapExpression : Expression {
  vector<pointer<MapEntry>> entries;
};

struct GroupedExpression : Expression {
  pointer<Expression> expression;
};

struct BinaryExpression : Expression {
  pointer<Expression> left;
  string op;
  pointer<Expression> right;
};

struct EvalStatement : Statement {
  pointer<Expression> expression;
};

struct LegacyEvalStatement : Statement {
  pointer<Expression> expression;
};

} // namespace ast
namespace {

using namespace pegium::parser;

std::string decode_string_literal(std::string_view text) {
  if (text.size() < 2u) {
    return std::string(text);
  }

  std::string result;
  result.reserve(text.size() - 2u);
  for (std::size_t index = 1; index + 1u < text.size(); ++index) {
    const auto current = text[index];
    if (current != '\\' || index + 2u >= text.size()) {
      result.push_back(current);
      continue;
    }

    const auto escaped = text[++index];
    switch (escaped) {
    case 'n':
      result.push_back('\n');
      break;
    case 'r':
      result.push_back('\r');
      break;
    case 't':
      result.push_back('\t');
      break;
    case '\\':
    case '"':
    case '\'':
      result.push_back(escaped);
      break;
    default:
      result.push_back(escaped);
      break;
    }
  }
  return result;
}

class AdversarialLanguageParser final : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  [[nodiscard]] const pegium::grammar::ParserRule &
  getEntryRule() const noexcept override {
    return ModelRule;
  }

  [[nodiscard]] const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Terminal<> INLINE_WS{"INLINE_WS", some(" "_kw | "\t"_kw)};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};
  Terminal<bool> FLAG{
      "FLAG", "yes"_kw.i() | "no"_kw.i(),
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<bool> {
        return opt::conversion_value<bool>(text == "yes" || text == "YES" ||
                                           text == "Yes" || text == "yEs" ||
                                           text == "yeS" || text == "YEs" ||
                                           text == "yES" || text == "YeS");
      })};
  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<std::string> {
        return opt::conversion_value<std::string>(decode_string_literal(text));
      })};

  Rule<std::string> ScopedName{"ScopedName", some(ID, "::"_kw)};
  Rule<std::string> ProbeMode{
      "ProbeMode", "fast"_kw.i() | "slow"_kw.i() | "deep"_kw.i()};

  Rule<ast::Model> ModelRule{
      "Model",
      "graph"_kw.i() + assign<&ast::Model::name>(ID) +
          many(append<&ast::Model::statements>(StatementRule))};

  Rule<ast::Statement> StatementRule{
      "Statement",
      NodeRule | AliasRule | LinkRule | MixRule | ProbeRule | PackRule |
          CaseRule | EvalRule | LegacyEvalRule};

  Rule<ast::Node> NodeRule{
      "Node",
      option(enable_if<&ast::Node::exported>("export"_kw.i())) +
          "node"_kw.i() + assign<&ast::Node::name>(ID) +
          option("extends"_kw.i() + assign<&ast::Node::superType>(ScopedName)) +
          option("when"_kw.i() + assign<&ast::Node::guard>(ExpressionRule)) +
          "{"_kw + many(append<&ast::Node::slots>(SlotRule)) + "}"_kw};

  Rule<ast::Slot> SlotRule{
      "Slot",
      option(enable_if<&ast::Slot::many>("many"_kw.i())) +
          assign<&ast::Slot::name>(ID) + ":"_kw +
          assign<&ast::Slot::type>(ScopedName) +
          option("="_kw + assign<&ast::Slot::fallback>(ExpressionRule)) +
          ";"_kw};

  Rule<ast::Alias> AliasRule{
      "Alias",
      "alias"_kw.i() + assign<&ast::Alias::name>(ID) + "="_kw +
          assign<&ast::Alias::target>(ScopedName) + ";"_kw};

  Rule<ast::Link> LinkRule{
      "Link",
      "link"_kw.i() + assign<&ast::Link::source>(ScopedName) + "->"_kw +
          assign<&ast::Link::target>(ScopedName) +
          option("when"_kw.i() + assign<&ast::Link::guard>(ExpressionRule)) +
          ";"_kw};

  Rule<ast::Mix> MixRule{
      "Mix",
      "mix"_kw.i() +
          (assign<&ast::Mix::hot>("hot"_kw.i()) &
           assign<&ast::Mix::cold>("cold"_kw.i()) &
           assign<&ast::Mix::warm>("warm"_kw.i()) &
           assign<&ast::Mix::dry>("dry"_kw.i())) +
          ";"_kw};

  Rule<ast::Probe> ProbeRule{
      "Probe",
      "probe"_kw.i() +
          ((&"fast"_kw.i()) + assign<&ast::Probe::mode>("fast"_kw.i()) |
           (&"slow"_kw.i()) + assign<&ast::Probe::mode>("slow"_kw.i()) |
           (&"deep"_kw.i()) + assign<&ast::Probe::mode>("deep"_kw.i())) +
          !("="_kw) + ";"_kw};

  Rule<ast::Pack> PackRule{
      "Pack",
      "pack"_kw.i() +
          ("["_kw + append<&ast::Pack::items>(ID) +
           many(","_kw + append<&ast::Pack::items>(ID)) + "]"_kw)
              .skip(ignored(INLINE_WS)) +
          ";"_kw};

  Rule<ast::CaseBlock> CaseRule{
      "Case",
      "case"_kw.i() + assign<&ast::CaseBlock::name>(ID) +
          option("when"_kw.i() + assign<&ast::CaseBlock::guard>(ExpressionRule)) +
          "{"_kw + many(append<&ast::CaseBlock::statements>(StatementRule)) +
          "}"_kw};

  Rule<ast::Expression> ExpressionRule{"Expression", BinaryExpressionRule};

  Infix<ast::BinaryExpression, &ast::BinaryExpression::left,
        &ast::BinaryExpression::op, &ast::BinaryExpression::right>
      BinaryExpressionRule{"BinaryExpression", PrimaryExpressionRule,
                           LeftAssociation("||"_kw),
                           LeftAssociation("&&"_kw),
                           LeftAssociation("+"_kw | "-"_kw),
                           LeftAssociation("*"_kw | "/"_kw)};

  Rule<ast::MapEntry> MapEntryRule{
      "MapEntry",
      assign<&ast::MapEntry::key>(ID) + ":"_kw +
          assign<&ast::MapEntry::value>(ExpressionRule)};

  Rule<ast::Expression> PrimaryExpressionRule{
      "PrimaryExpression",
      create<ast::GroupedExpression>() + "("_kw +
          assign<&ast::GroupedExpression::expression>(ExpressionRule) +
          ")"_kw |
          create<ast::MapExpression>() + "{"_kw +
              option(append<&ast::MapExpression::entries>(MapEntryRule) +
                     many(","_kw +
                          append<&ast::MapExpression::entries>(MapEntryRule))) +
              "}"_kw |
          create<ast::ListExpression>() +
              ("["_kw +
               option(append<&ast::ListExpression::items>(ExpressionRule) +
                      many(","_kw +
                           append<&ast::ListExpression::items>(ExpressionRule))) +
               "]"_kw)
                  .skip(ignored(INLINE_WS)) |
          create<ast::CallExpression>() +
              assign<&ast::CallExpression::callee>(ScopedName) +
              option(("<"_kw +
                      append<&ast::CallExpression::typeArgs>(ScopedName) +
                      many(","_kw +
                           append<&ast::CallExpression::typeArgs>(ScopedName)) +
                      ">"_kw)
                         .skip(ignored(INLINE_WS))) +
              "("_kw +
              option(append<&ast::CallExpression::args>(ExpressionRule) +
                     many(","_kw +
                          append<&ast::CallExpression::args>(ExpressionRule))) +
              ")"_kw |
          create<ast::ReferenceExpression>() +
              assign<&ast::ReferenceExpression::target>(ScopedName) |
          create<ast::StringExpression>() +
              assign<&ast::StringExpression::value>(STRING) |
          create<ast::BooleanExpression>() +
              assign<&ast::BooleanExpression::value>(FLAG) |
          create<ast::NumberExpression>() +
              assign<&ast::NumberExpression::value>(NUMBER)};

  Rule<ast::EvalStatement> EvalRule{
      "Eval",
      "eval"_kw.i() + assign<&ast::EvalStatement::expression>(ExpressionRule) +
          ";"_kw};

  Rule<ast::Expression> LegacyExpressionRule{
      "LegacyExpression",
      LegacyPrimaryRule +
          many(nest<&ast::BinaryExpression::left>() +
               assign<&ast::BinaryExpression::op>(
                   "+"_kw | "-"_kw | "*"_kw | "&&"_kw) +
               assign<&ast::BinaryExpression::right>(LegacyPrimaryRule))};

  Rule<ast::Expression> LegacyPrimaryRule{
      "LegacyPrimary",
      create<ast::GroupedExpression>() + "("_kw +
          assign<&ast::GroupedExpression::expression>(LegacyExpressionRule) +
          ")"_kw |
          create<ast::CallExpression>() +
              assign<&ast::CallExpression::callee>(ScopedName) + "("_kw +
              option(append<&ast::CallExpression::args>(LegacyExpressionRule) +
                     many(","_kw +
                          append<&ast::CallExpression::args>(
                              LegacyExpressionRule))) +
              ")"_kw |
          create<ast::ReferenceExpression>() +
              assign<&ast::ReferenceExpression::target>(ScopedName) |
          create<ast::NumberExpression>() +
              assign<&ast::NumberExpression::value>(NUMBER)};

  Rule<ast::LegacyEvalStatement> LegacyEvalRule{
      "LegacyEval",
      "legacy"_kw.i() +
          assign<&ast::LegacyEvalStatement::expression>(LegacyExpressionRule) +
          ";"_kw};
#pragma clang diagnostic pop
};

[[nodiscard]] bool has_slot(const ast::Node &node) noexcept {
  return std::ranges::any_of(node.slots,
                             [](const auto &slot) { return static_cast<bool>(slot); });
}

} // namespace

bool register_adversarial_language_services(
    pegium::SharedServices &sharedServices) {
  auto services =
      pegium::makeDefaultServices(sharedServices, "adversarial-language");
  services->parser = std::make_unique<const AdversarialLanguageParser>(*services);
  services->languageMetaData.fileExtensions = {".adv"};

  auto &registry = *services->validation.validationRegistry;
  registry.registerCheck<ast::Node>(
      [](const ast::Node &node,
         const pegium::validation::ValidationAcceptor &accept) {
        if (!has_slot(node)) {
          accept.warning(node, "Node should declare at least one slot.")
              .property<&ast::Node::name>();
        }
        if (node.superType.has_value() && node.superType->get() == &node) {
          accept.error(node, "Node cannot extend itself.")
              .property<&ast::Node::superType>();
        }
      });
  registry.registerCheck<ast::Pack>(
      [](const ast::Pack &pack,
         const pegium::validation::ValidationAcceptor &accept) {
        for (std::size_t left = 0; left < pack.items.size(); ++left) {
          for (std::size_t right = left + 1u; right < pack.items.size();
               ++right) {
            if (pack.items[left] != pack.items[right]) {
              continue;
            }
            accept.warning(pack, "Duplicate pack item.")
                .property<&ast::Pack::items>(right);
          }
        }
      });
  registry.registerCheck<ast::Link>(
      [](const ast::Link &link,
         const pegium::validation::ValidationAcceptor &accept) {
        if (link.source.get() == link.target.get()) {
          accept.warning(link, "Self links are suspicious.")
              .property<&ast::Link::target>();
        }
      });
  registry.registerCheck<ast::CaseBlock>(
      [](const ast::CaseBlock &block,
         const pegium::validation::ValidationAcceptor &accept) {
        if (block.statements.empty()) {
          accept.warning(block, "Case block should not be empty.")
              .property<&ast::CaseBlock::name>();
        }
      });

  sharedServices.serviceRegistry->registerServices(std::move(services));
  return true;
}

} // namespace pegium::fuzz::adversarial
