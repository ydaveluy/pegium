#include <fuzz/StressLanguage.hpp>

#include <algorithm>
#include <memory>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/validation/ValidationRegistry.hpp>
#include <pegium/lsp/services/DefaultLspModule.hpp>

namespace pegium::fuzz::stress {
namespace ast {

struct Statement;
struct Declaration;
struct Field;
struct Use;
struct Choice;
struct Bag;
struct Peek;
struct Guard;
struct Path;
struct Doc;
struct Setting;
struct Tuple;
struct Expression;
struct NumberExpression;
struct BooleanExpression;
struct ReferenceExpression;
struct CallExpression;
struct GroupedExpression;
struct BinaryExpression;
struct ExpressionStatement;
struct LegacyExpressionStatement;

struct Model : pegium::AstNode {
  string name;
  vector<pointer<Statement>> statements;
};

struct Statement : pegium::AstNode {};

struct Declaration : Statement {
  bool exported = false;
  string name;
  optional<reference<Declaration>> superType;
  vector<pointer<Field>> members;
};

struct Field : pegium::AstNode {
  bool many = false;
  string name;
  reference<Declaration> type;
};

struct Use : Statement {
  vector<reference<Declaration>> targets;
  optional<reference<Declaration>> fallback;
};

struct Choice : Statement {
  bool exported = false;
  string kind;
};

struct Bag : Statement {
  bool alpha = false;
  bool beta = false;
  bool gamma = false;
};

struct Peek : Statement {
  string branch;
};

struct Guard : Statement {
  string name;
};

struct Path : Statement {
  string value;
};

struct Doc : Statement {
  string text;
};

struct Setting : Statement {
  string name;
  bool enabled = false;
};

struct Tuple : Statement {
  vector<string> items;
};

struct Expression : pegium::AstNode {};

struct NumberExpression : Expression {
  double value = 0.0;
};

struct BooleanExpression : Expression {
  bool value = false;
};

struct ReferenceExpression : Expression {
  reference<Declaration> target;
};

struct CallExpression : Expression {
  reference<Declaration> callee;
  vector<pointer<Expression>> args;
};

struct GroupedExpression : Expression {
  pointer<Expression> expression;
};

struct BinaryExpression : Expression {
  pointer<Expression> left;
  string op;
  pointer<Expression> right;
};

struct ExpressionStatement : Statement {
  pointer<Expression> expression;
};

struct LegacyExpressionStatement : Statement {
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

class StressLanguageParser final : public PegiumParser {
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
  Terminal<bool> TOGGLE{
      "TOGGLE", "on"_kw.i() | "off"_kw.i(),
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<bool> {
        return opt::conversion_value<bool>(text == "on" || text == "ON" ||
                                           text == "On" || text == "oN");
      })};
  Terminal<std::string> STRING{
      "STRING",
      ("\""_kw + many("\\"_kw + dot | !"\""_kw + dot) + "\""_kw) |
          ("'"_kw + many("\\"_kw + dot | !"'"_kw + dot) + "'"_kw),
      opt::with_converter([](std::string_view text) noexcept
                              -> opt::ConversionResult<std::string> {
        return opt::conversion_value<std::string>(decode_string_literal(text));
      })};

  Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};
  Rule<std::string> ChoiceKind{
      "ChoiceKind", "entity"_kw.i() | "event"_kw.i() | "command"_kw.i()};

  Rule<ast::Model> ModelRule{
      "Model",
      "module"_kw.i() + assign<&ast::Model::name>(ID) +
          many(append<&ast::Model::statements>(StatementRule))};

  Rule<ast::Statement> StatementRule{
      "Statement",
      DeclarationRule | UseRule | ChoiceRule | BagRule | PeekRule | GuardRule |
          PathRule | DocRule | SettingRule | TupleRule |
          ExpressionStatementRule | LegacyExpressionStatementRule};

  Rule<ast::Declaration> DeclarationRule{
      "Declaration",
      option(enable_if<&ast::Declaration::exported>("export"_kw.i())) +
          "decl"_kw.i() + assign<&ast::Declaration::name>(ID) +
          option("extends"_kw.i() + assign<&ast::Declaration::superType>(
                                       QualifiedName)) +
          "{"_kw + many(append<&ast::Declaration::members>(FieldRule)) +
          "}"_kw};

  Rule<ast::Field> FieldRule{
      "Field",
      option(enable_if<&ast::Field::many>("many"_kw.i())) +
          assign<&ast::Field::name>(ID) + ":"_kw +
          assign<&ast::Field::type>(QualifiedName) + ";"_kw};

  Rule<ast::Use> UseRule{
      "Use",
      "use"_kw.i() + append<&ast::Use::targets>(QualifiedName) +
          many(","_kw + append<&ast::Use::targets>(QualifiedName)) +
          option("fallback"_kw.i() + assign<&ast::Use::fallback>(QualifiedName)) +
          ";"_kw};

  Rule<ast::Choice> ChoiceRule{
      "Choice",
      option(enable_if<&ast::Choice::exported>("export"_kw.i())) +
          "choose"_kw.i() + assign<&ast::Choice::kind>(ChoiceKind) + ";"_kw};

  Rule<ast::Bag> BagRule{
      "Bag",
      "bag"_kw.i() +
          (assign<&ast::Bag::alpha>("alpha"_kw.i()) &
           assign<&ast::Bag::beta>("beta"_kw.i()) &
           assign<&ast::Bag::gamma>("gamma"_kw.i())) +
          ";"_kw};

  Rule<ast::Peek> PeekRule{
      "Peek",
      "peek"_kw.i() +
          ((&"alpha"_kw.i()) + assign<&ast::Peek::branch>("alpha"_kw.i()) |
           (&"beta"_kw.i()) + assign<&ast::Peek::branch>("beta"_kw.i()) |
           (&"gamma"_kw.i()) + assign<&ast::Peek::branch>("gamma"_kw.i())) +
          ";"_kw};

  Rule<ast::Guard> GuardRule{
      "Guard",
      "guard"_kw.i() + !("="_kw) + assign<&ast::Guard::name>(ID) + ";"_kw};

  Rule<ast::Path> PathRule{
      "Path", "path"_kw.i() + assign<&ast::Path::value>(QualifiedName) + ";"_kw};

  Rule<ast::Doc> DocRule{
      "Doc", "doc"_kw.i() + assign<&ast::Doc::text>(STRING) + ";"_kw};

  Rule<ast::Setting> SettingRule{
      "Setting",
      "setting"_kw.i() + assign<&ast::Setting::name>(ID) + "="_kw +
          assign<&ast::Setting::enabled>(TOGGLE) + ";"_kw};

  Rule<ast::Tuple> TupleRule{
      "Tuple",
      "tuple"_kw.i() +
          ("("_kw + append<&ast::Tuple::items>(ID) +
           many(","_kw + append<&ast::Tuple::items>(ID)) + ")"_kw)
              .skip(ignored(INLINE_WS)) +
          ";"_kw};

  Rule<ast::Expression> ExpressionRule{"Expression", BinaryExpressionRule};

  Infix<ast::BinaryExpression, &ast::BinaryExpression::left,
        &ast::BinaryExpression::op, &ast::BinaryExpression::right>
      BinaryExpressionRule{"BinaryExpression", PrimaryExpressionRule,
                           LeftAssociation("*"_kw | "/"_kw),
                           LeftAssociation("+"_kw | "-"_kw)};

  Rule<ast::Expression> PrimaryExpressionRule{
      "PrimaryExpression",
      create<ast::GroupedExpression>() + "("_kw +
          assign<&ast::GroupedExpression::expression>(ExpressionRule) +
          ")"_kw |
          create<ast::NumberExpression>() +
              assign<&ast::NumberExpression::value>(NUMBER) |
          create<ast::BooleanExpression>() +
              assign<&ast::BooleanExpression::value>(TOGGLE) |
          create<ast::CallExpression>() +
              assign<&ast::CallExpression::callee>(QualifiedName) + "("_kw +
              option(append<&ast::CallExpression::args>(ExpressionRule) +
                     many(","_kw +
                          append<&ast::CallExpression::args>(ExpressionRule))) +
              ")"_kw |
          create<ast::ReferenceExpression>() +
              assign<&ast::ReferenceExpression::target>(QualifiedName)};

  Rule<ast::ExpressionStatement> ExpressionStatementRule{
      "ExpressionStatement",
      "expr"_kw.i() + assign<&ast::ExpressionStatement::expression>(ExpressionRule) +
          ";"_kw};

  Rule<ast::Expression> LegacyExpressionRule{
      "LegacyExpression",
      LegacyPrimaryRule +
          many(nest<&ast::BinaryExpression::left>() +
               assign<&ast::BinaryExpression::op>("+"_kw | "-"_kw | "*"_kw) +
               assign<&ast::BinaryExpression::right>(LegacyPrimaryRule))};

  Rule<ast::Expression> LegacyPrimaryRule{
      "LegacyPrimary",
      create<ast::GroupedExpression>() + "("_kw +
          assign<&ast::GroupedExpression::expression>(LegacyExpressionRule) +
          ")"_kw |
          create<ast::NumberExpression>() +
              assign<&ast::NumberExpression::value>(NUMBER) |
          create<ast::ReferenceExpression>() +
              assign<&ast::ReferenceExpression::target>(QualifiedName)};

  Rule<ast::LegacyExpressionStatement> LegacyExpressionStatementRule{
      "LegacyExpressionStatement",
      "legacy"_kw.i() +
          assign<&ast::LegacyExpressionStatement::expression>(LegacyExpressionRule) +
          ";"_kw};
#pragma clang diagnostic pop
};

[[nodiscard]] bool has_member(const ast::Declaration &declaration) noexcept {
  return std::ranges::any_of(declaration.members, [](const auto &member) {
    return static_cast<bool>(member);
  });
}

} // namespace

bool register_stress_language_services(
    pegium::SharedServices &sharedServices) {
  auto services =
      pegium::makeDefaultServices(sharedServices, "stress-language");
  services->parser = std::make_unique<const StressLanguageParser>(*services);
  services->languageMetaData.fileExtensions = {".stress"};

  auto &registry = *services->validation.validationRegistry;
  registry.registerCheck<ast::Declaration>(
      [](const ast::Declaration &declaration,
         const pegium::validation::ValidationAcceptor &accept) {
        if (!has_member(declaration)) {
          accept.warning(declaration,
                         "Declaration should declare at least one member.")
              .property<&ast::Declaration::name>();
        }
        if (declaration.superType.has_value() &&
            declaration.superType->get() == &declaration) {
          accept.error(declaration, "Declaration cannot extend itself.")
              .property<&ast::Declaration::superType>();
        }
      });
  registry.registerCheck<ast::Tuple>(
      [](const ast::Tuple &tuple,
         const pegium::validation::ValidationAcceptor &accept) {
        for (std::size_t left = 0; left < tuple.items.size(); ++left) {
          for (std::size_t right = left + 1u; right < tuple.items.size();
               ++right) {
            if (tuple.items[left] != tuple.items[right]) {
              continue;
            }
            accept.warning(tuple, "Duplicate tuple item.")
                .property<&ast::Tuple::items>(right);
          }
        }
      });
  registry.registerCheck<ast::Setting>(
      [](const ast::Setting &setting,
         const pegium::validation::ValidationAcceptor &accept) {
        if (setting.name == "unsafe" && !setting.enabled) {
          accept.warning(setting, "Unsafe setting should stay enabled.")
              .property<&ast::Setting::enabled>();
        }
      });

  sharedServices.serviceRegistry->registerServices(std::move(services));
  return true;
}

} // namespace pegium::fuzz::stress
