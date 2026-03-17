#include "xsmp.hpp"
#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>
#include <sstream>
#include <stdexcept>

namespace Xsmp {

using namespace pegium::parser;
class XsmpParser : public PegiumParser {
public:
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Catalogue;
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

  Rule<> QualifiedName{"QualifiedName", some(ID, "."_kw)};

  static constexpr auto Visibility =
      "private"_kw | "protected"_kw | "public"_kw;

  Rule<Xsmp::Attribute> Attribute{
      "Attribute",
      "@"_kw + assign<&Attribute::type>(QualifiedName) +
          option("("_kw + option(assign<&Attribute::value>(Expression)) +
                 ")"_kw)};
  Rule<Xsmp::Catalogue> Catalogue{
      "Catalogue",
      many(append<&NamedElement::attributes>(Attribute)) +   //
          "catalogue"_kw + assign<&NamedElement::name>(ID) + //
          many(append<&Catalogue::namespaces>(RootNamespace))};

  Rule<Xsmp::Namespace> RootNamespace{
      "RootNamespace",
      many(append<&NamedElement::attributes>(Attribute)) + Namespace};

  Rule<Xsmp::Namespace> Namespace{
      "Namespace", "namespace"_kw + assign<&NamedElement::name>(ID) +       //
                       "{"_kw +                                             //
                       many(append<&Namespace::members>(NamespaceMember)) + //
                       "}"_kw};                                             //

  Rule<Xsmp::NamedElement> NamespaceMember{
      "NamespaceMember",
      // attributes
      many(append<&NamedElement::attributes>(Attribute)) +
          (
              // Namespace
              Namespace |
              // elements with visibility
              many(append<&VisibilityElement::modifiers>(Visibility)) +
                  (
                      // Types
                      Structure | Interface | Array | ValueReference | Float |
                      Integer | EventType | StringType | PrimitiveType |
                      NativeType | AttributeType | Enumeration |
                      // Types with abstract
                      option(
                          append<&VisibilityElement::modifiers>("abstract"_kw) +
                          many(append<&VisibilityElement::modifiers>(
                              Visibility | "abstract"_kw))) +
                          (Class | Exception | Model | Service) //
                      )
              //
              )};
  Rule<Xsmp::Enumeration> Enumeration{
      "Enumeration",
      "enum"_kw + assign<&NamedElement::name>(ID) +                          //
          "{"_kw +                                                           //
          many(append<&Enumeration::literals>(EnumerationLiteral), ","_kw) + //
          "}"_kw};

  Rule<Xsmp::EnumerationLiteral> EnumerationLiteral{
      "EnumerationLiteral", assign<&NamedElement::name>(ID) + "="_kw +
                                assign<&EnumerationLiteral::value>(Expression)};

  Rule<Xsmp::Structure> Structure{
      "Structure", "struct"_kw + assign<&NamedElement::name>(ID) +          //
                       "{"_kw +                                             //
                       many(append<&Structure::members>(StructureMember)) + //
                       "}"_kw};

  Rule<Xsmp::NamedElement> StructureMember{
      "StructureMember",
      many(append<&NamedElement::attributes>(Attribute)) +
          many(append<&VisibilityElement::modifiers>(Visibility)) +
          (Field | Constant)};

  Rule<Xsmp::Class> Class{
      "Class", "class"_kw + assign<&NamedElement::name>(ID) +       //
                   "{"_kw +                                         //
                   many(append<&Structure::members>(ClassMember)) + //
                   "}"_kw};

  Rule<Xsmp::NamedElement> ClassMember{
      "ClassMember",
      many(append<&NamedElement::attributes>(Attribute)) +
          many(append<&VisibilityElement::modifiers>(Visibility)) +
          (Field | Constant | Property | Association)};

  Rule<Xsmp::Exception> Exception{
      "Exception", "exception"_kw + assign<&NamedElement::name>(ID) +   //
                       "{"_kw +                                         //
                       many(append<&Structure::members>(ClassMember)) + //
                       "}"_kw};

  Rule<Xsmp::Interface> Interface{
      "Interface",
      "interface"_kw + assign<&NamedElement::name>(ID) +
          // base interfaces
          option("extends"_kw +
                 some(append<&Interface::bases>(QualifiedName), ","_kw)) + //
          "{"_kw +                                                         //
          many(append<&Structure::members>(InterfaceMember)) +             //
          "}"_kw};

  Rule<Xsmp::NamedElement> InterfaceMember{
      "InterfaceMember",
      many(append<&NamedElement::attributes>(Attribute)) +
          many(append<&VisibilityElement::modifiers>(Visibility)) +
          (Constant | Property)};

  Rule<Xsmp::Model> Model{
      "Model",
      "model"_kw + assign<&NamedElement::name>(ID) +
          // base class
          option("extends"_kw + assign<&Component::base>(QualifiedName)) +
          // base interfaces
          option(
              "implements"_kw +
              some(append<&Component::interfaces>(QualifiedName), ","_kw)) + //
          "{"_kw +                                                           //
          many(append<&Component::members>(ComponentMember)) +               //
          "}"_kw};

  Rule<Xsmp::Service> Service{
      "Service",
      "service"_kw + assign<&NamedElement::name>(ID) +
          // base class
          option("extends"_kw + assign<&Component::base>(QualifiedName)) +
          // base interfaces
          option(
              "implements"_kw +
              some(append<&Component::interfaces>(QualifiedName), ","_kw)) + //
          "{"_kw +                                                           //
          many(append<&Component::members>(ComponentMember)) +               //
          "}"_kw};

  Rule<Xsmp::NamedElement> ComponentMember{
      "ComponentMember",
      many(append<&NamedElement::attributes>(Attribute)) +
          (
              // elemenst with visibility
              many(append<&VisibilityElement::modifiers>(Visibility)) +
                  (Constant | Association | Field | Property) |
              Container | Reference)};

  Rule<Xsmp::Array> Array{
      "Array", "array"_kw + assign<&NamedElement::name>(ID) + "="_kw +
                   assign<&Array::itemType>(QualifiedName) + "["_kw +
                   assign<&Array::size>(Expression) + "]"_kw};

  Rule<Xsmp::StringType> StringType{
      "StringType", "string"_kw + assign<&NamedElement::name>(ID) + "="_kw +
                        "["_kw + assign<&StringType::size>(Expression) +
                        "]"_kw};

  Rule<Xsmp::ValueReference> ValueReference{
      "ValueReference", "using"_kw + assign<&NamedElement::name>(ID) + "="_kw +
                            assign<&ValueReference::type>(QualifiedName) +
                            "*"_kw};

  Rule<Xsmp::Float> Float{
      "Float",
      "float"_kw + assign<&NamedElement::name>(ID) +
          option("extends"_kw + assign<&Float::primitiveType>(QualifiedName))
      // TODO add min/max
  };

  Rule<Xsmp::Integer> Integer{
      "Integer",
      "integer"_kw + assign<&NamedElement::name>(ID) +
          option("extends"_kw + assign<&Integer::primitiveType>(QualifiedName))
      // TODO add min/max
  };
  Rule<Xsmp::EventType> EventType{
      "EventType",
      "event"_kw + assign<&NamedElement::name>(ID) +
          option("extends"_kw + assign<&EventType::eventArg>(QualifiedName))};

  Rule<Xsmp::PrimitiveType> PrimitiveType{
      "PrimitiveType", "primitive"_kw + assign<&NamedElement::name>(ID)};

  Rule<Xsmp::NativeType> NativeType{
      "NativeType", "native"_kw + assign<&NamedElement::name>(ID)};

  Rule<Xsmp::AttributeType> AttributeType{
      "AttributeType",
      "attribute"_kw + assign<&AttributeType::type>(QualifiedName) +
          assign<&NamedElement::name>(ID) +
          option("="_kw + assign<&AttributeType::value>(Expression))};

  Rule<Xsmp::Constant> Constant{
      "Constant", "constant"_kw + assign<&Constant::type>(QualifiedName) +
                      assign<&NamedElement::name>(ID) + "="_kw +
                      assign<&Constant::value>(Expression)};

  Rule<Xsmp::Field> Field{
      "Field",
      option(append<&VisibilityElement::modifiers>("input"_kw | "output"_kw |
                                                   "transient"_kw) +
             many(append<&VisibilityElement::modifiers>(
                 Visibility | "input"_kw | "output"_kw | "transient"_kw))) +
          "field"_kw + assign<&Field::type>(QualifiedName) +
          assign<&NamedElement::name>(ID) +
          option("="_kw + assign<&Field::value>(Expression))};

  Rule<Xsmp::Property> Property{
      "Property", option(append<&VisibilityElement::modifiers>(
                             "readWrite"_kw | "readOnly"_kw | "writeOnly"_kw) +
                         many(append<&VisibilityElement::modifiers>(
                             Visibility | "readWrite"_kw | "readOnly"_kw |
                             "writeOnly"_kw))) +
                      "property"_kw + assign<&Property::type>(QualifiedName) +
                      assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Association> Association{
      "Association", "association"_kw +
                         assign<&Association::type>(QualifiedName) +
                         assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Container> Container{
      "Container", "container"_kw + assign<&Container::type>(QualifiedName) +
                       assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Reference> Reference{
      "Reference", "reference"_kw + assign<&Reference::type>(QualifiedName) +
                       assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Expression> Expression{"Expression", BinaryExpression};

  Infix<Xsmp::BinaryExpression, &Xsmp::BinaryExpression::leftOperand,
            &Xsmp::BinaryExpression::feature,
            &Xsmp::BinaryExpression::rightOperand>
      BinaryExpression{"BinaryExpression", PrimaryExpression,
                       LeftAssociation("&&"_kw), LeftAssociation("||"_kw)};

  Rule<Xsmp::Expression> PrimaryExpression{"PrimaryExpression", BooleanLiteral};

  Rule<Xsmp::BooleanLiteral> BooleanLiteral{
      "BooleanLiteral",
      enable_if<&BooleanLiteral::isTrue>("true"_kw) | "false"_kw};

#pragma clang diagnostic pop
};
} // namespace Xsmp

namespace {

constexpr std::string_view kXsmpTypoCatalogueBenchmarkName =
    "xsmp-typo-catalogue";
constexpr std::string_view kXsmpTypoCatalogueGateKey =
    "xsmp_typo_catalogue";

std::string makeXsmpPayload(std::size_t repetitions) {
  std::string input = R"(
    /**
     * A demo catalogue
     */
    catalogue test 

  )";
  for (std::size_t i = 0; i < repetitions; ++i) {
    input += R"(    
   
namespace hidden
{
    
    primitive Bool
    integer privateInteger extends Smp.Int32
    public private attribute Bool SingleAttribute 
    service MyService extends Other.Service implements interfaces.I1, interfaces.I2
    {
        @Static
        @Virtual
        @Abstract
        @Const
        readOnly public readWrite property Int32 property 

        reference MyInterface ref8

       container MyService ctn 
       input output transient field Bool aField
       input output transient field Bool aField = false
    }
    enum MyEnum2
    {
        L0 = false,
        L1 = true
    }

    array MyArray = MyModel2[true]
}

    )";
  }
  return input;
}

const Xsmp::BooleanLiteral *asBoolLiteral(const Xsmp::Expression *expr) {
  return dynamic_cast<const Xsmp::BooleanLiteral *>(expr);
}

const Xsmp::BinaryExpression *asBinary(const Xsmp::Expression *expr) {
  return dynamic_cast<const Xsmp::BinaryExpression *>(expr);
}

std::string makeCatalogueReferenceText() {
  return R"(
    catalogue Demo
    namespace Core
    {
      primitive Bool
      integer Int32 extends Smp.Int32
      @Meta
      attribute Bool Flag = true
      public abstract service MyService extends Base.Service implements IOne, ITwo
      {
        reference Bool refValue
        container Bool bag
        public constant Bool enabled = true
        input output field Bool flag = false
      }
      enum Mode
      {
        ON = true,
        OFF = false
      }
    }
  )";
}

std::string replaceOnce(std::string input, std::string_view from,
                        std::string_view to) {
  const auto pos = input.find(from);
  if (pos == std::string::npos) {
    throw std::logic_error("replaceOnce: pattern not found");
  }
  input.replace(pos, from.size(), to);
  return input;
}

void assertCatalogueAst(const Xsmp::Catalogue *catalogue) {
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_EQ(catalogue->name, "Demo");
  ASSERT_EQ(catalogue->namespaces.size(), 1u);
  ASSERT_TRUE(catalogue->namespaces[0] != nullptr);

  const auto &ns = catalogue->namespaces[0];

  EXPECT_EQ(ns->name, "Core");

  ASSERT_EQ(ns->members.size(), 5u);

  const auto *primitive =
      dynamic_cast<const Xsmp::PrimitiveType *>(ns->members[0].get());
  ASSERT_NE(primitive, nullptr);
  EXPECT_EQ(primitive->name, "Bool");

  const auto *integer =
      dynamic_cast<const Xsmp::Integer *>(ns->members[1].get());
  ASSERT_NE(integer, nullptr);
  EXPECT_EQ(integer->name, "Int32");
  EXPECT_TRUE(integer->primitiveType.has_value());

  const auto *attributeType =
      dynamic_cast<const Xsmp::AttributeType *>(ns->members[2].get());
  ASSERT_NE(attributeType, nullptr);
  EXPECT_EQ(attributeType->name, "Flag");
  ASSERT_EQ(attributeType->attributes.size(), 1u);
  ASSERT_TRUE(attributeType->attributes[0] != nullptr);
  ASSERT_TRUE(attributeType->value != nullptr);
  const auto *attributeValue = asBoolLiteral(attributeType->value.get());
  ASSERT_NE(attributeValue, nullptr);
  EXPECT_TRUE(attributeValue->isTrue);

  const auto *service = dynamic_cast<Xsmp::Service *>(ns->members[3].get());
  ASSERT_TRUE(service != nullptr);
  EXPECT_EQ(service->name, "MyService");
  ASSERT_EQ(service->modifiers.size(), 2u);
  EXPECT_EQ(service->modifiers[0], "public");
  EXPECT_EQ(service->modifiers[1], "abstract");
  EXPECT_TRUE(service->base.has_value());
  ASSERT_EQ(service->interfaces.size(), 2u);
  ASSERT_EQ(service->members.size(), 4u);

  const auto *reference =
      dynamic_cast<const Xsmp::Reference *>(service->members[0].get());
  ASSERT_NE(reference, nullptr);
  EXPECT_EQ(reference->name, "refValue");

  const auto *container =
      dynamic_cast<const Xsmp::Container *>(service->members[1].get());
  ASSERT_NE(container, nullptr);
  EXPECT_EQ(container->name, "bag");

  const auto *constant =
      dynamic_cast<const Xsmp::Constant *>(service->members[2].get());
  ASSERT_NE(constant, nullptr);
  EXPECT_EQ(constant->name, "enabled");
  ASSERT_EQ(constant->modifiers.size(), 1u);
  EXPECT_EQ(constant->modifiers[0], "public");
  ASSERT_TRUE(constant->value != nullptr);
  const auto *constantValue = asBoolLiteral(constant->value.get());
  ASSERT_NE(constantValue, nullptr);
  EXPECT_TRUE(constantValue->isTrue);

  const auto *field =
      dynamic_cast<const Xsmp::Field *>(service->members[3].get());
  ASSERT_NE(field, nullptr);
  EXPECT_EQ(field->name, "flag");
  ASSERT_EQ(field->modifiers.size(), 2u);
  EXPECT_EQ(field->modifiers[0], "input");
  EXPECT_EQ(field->modifiers[1], "output");
  ASSERT_TRUE(field->value != nullptr);
  const auto *fieldValue = asBoolLiteral(field->value.get());
  ASSERT_NE(fieldValue, nullptr);
  EXPECT_FALSE(fieldValue->isTrue);

  const auto *enumeration =
      dynamic_cast<const Xsmp::Enumeration *>(ns->members[4].get());
  ASSERT_NE(enumeration, nullptr);
  EXPECT_EQ(enumeration->name, "Mode");

  ASSERT_EQ(enumeration->literals.size(), 2u);

  ASSERT_TRUE(enumeration->literals[0] != nullptr);
  EXPECT_EQ(enumeration->literals[0]->name, "ON");
  ASSERT_TRUE(enumeration->literals[0]->value != nullptr);
  const auto *onValue = asBoolLiteral(enumeration->literals[0]->value.get());
  ASSERT_NE(onValue, nullptr);
  EXPECT_TRUE(onValue->isTrue);
  ASSERT_TRUE(enumeration->literals[1] != nullptr);
  EXPECT_EQ(enumeration->literals[1]->name, "OFF");
  ASSERT_TRUE(enumeration->literals[1]->value != nullptr);
  const auto *offValue = asBoolLiteral(enumeration->literals[1]->value.get());
  ASSERT_NE(offValue, nullptr);
  EXPECT_FALSE(offValue->isTrue);
}

void expectDiagnosticKind(const pegium::parser::ParseResult &parsed,
                          pegium::parser::ParseDiagnosticKind kind) {
  const bool hasKind =
      std::ranges::any_of(parsed.parseDiagnostics,
                          [kind](const pegium::parser::ParseDiagnostic &diag) {
                            return diag.kind == kind;
                          });
  if (!hasKind) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &diag : parsed.parseDiagnostics) {
      if (!first) {
        oss << ", ";
      }
      first = false;
      oss << static_cast<int>(diag.kind) << '@' << diag.offset;
    }
    ADD_FAILURE() << "expected diagnostic kind=" << static_cast<int>(kind)
                  << " but got [" << oss.str() << ']';
  }
}

template <typename ParserType>
auto
parse_text(const ParserType &parser, std::string_view text,
           const pegium::parser::ParseOptions &options = {}) {
  (void)options;
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  parser.parse(*document);
  return std::move(document);
}

void printParseReport(std::string_view name,
                      const pegium::parser::ParseResult &parsed) {
  const auto &report = parsed.recoveryReport;
  std::cout << "[parse-report] " << name << ": full=" << parsed.fullMatch
            << " diagnostics=" << parsed.parseDiagnostics.size()
            << " strictRuns=" << report.strictParseRuns
            << " recoveryAttempts=" << report.recoveryAttemptRuns
            << " windowsTried=" << report.recoveryWindowsTried
            << " windowsSelected=" << report.recoveryCount
            << " edits=" << report.recoveryEdits << '\n';
}

} // namespace

TEST(XsmpTest, ParseBuildsExpectedAstForSmallCatalogue) {
  Xsmp::XsmpParser parser;

  const std::string input = makeCatalogueReferenceText();

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  EXPECT_TRUE(parsed.parseDiagnostics.empty());
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  assertCatalogueAst(catalogue);
}

TEST(XsmpTest, ParsesBinaryExpressionWithExpectedPrecedence) {
  Xsmp::XsmpParser parser;

  const std::string input = R"(
    catalogue Demo
    namespace Core
    {
      primitive Bool
      attribute Bool Flag = true || false && true
    }
  )";

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  EXPECT_TRUE(parsed.parseDiagnostics.empty());
  ASSERT_TRUE(parsed.value);

  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  ASSERT_EQ(catalogue->namespaces.size(), 1u);
  ASSERT_TRUE(catalogue->namespaces[0] != nullptr);

  const auto &ns = catalogue->namespaces[0];
  ASSERT_EQ(ns->members.size(), 2u);

  const auto *attributeType =
      dynamic_cast<const Xsmp::AttributeType *>(ns->members[1].get());
  ASSERT_NE(attributeType, nullptr);
  ASSERT_TRUE(attributeType->value != nullptr);

  const auto *orExpression = asBinary(attributeType->value.get());
  ASSERT_NE(orExpression, nullptr);
  EXPECT_EQ(orExpression->feature, "||");
  ASSERT_TRUE(orExpression->leftOperand != nullptr);
  ASSERT_TRUE(orExpression->rightOperand != nullptr);
  ASSERT_NE(asBoolLiteral(orExpression->leftOperand.get()), nullptr);

  const auto *andExpression = asBinary(orExpression->rightOperand.get());
  ASSERT_NE(andExpression, nullptr);
  EXPECT_EQ(andExpression->feature, "&&");
  ASSERT_TRUE(andExpression->leftOperand != nullptr);
  ASSERT_TRUE(andExpression->rightOperand != nullptr);
  ASSERT_NE(asBoolLiteral(andExpression->leftOperand.get()), nullptr);
  ASSERT_NE(asBoolLiteral(andExpression->rightOperand.get()), nullptr);
}

TEST(XsmpTest, GenericRecoveryRepairsCatalogueKeywordSingleSubstitution) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "catalogue", "catalogoe");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());

  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  assertCatalogueAst(catalogue);
}

TEST(XsmpTest, GenericRecoveryRepairsCatalogueKeywordMultiEditAtRoot) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "catalogue", "catalogxoe");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_TRUE(parsed.fullMatch);
  EXPECT_FALSE(parsed.parseDiagnostics.empty());

  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  assertCatalogueAst(catalogue);
}

TEST(XsmpTest, GenericRecoveryLeavesPartialAstForTypoInServiceHeader) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "implements", "implxments");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_TRUE(catalogue->namespaces.empty());
}

TEST(XsmpTest, GenericRecoveryLeavesPartialAstForTypoInServiceBody) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "constant", "constxnt");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_TRUE(catalogue->namespaces.empty());
}

TEST(XsmpTest, GenericRecoveryLeavesPartialAstForTypoInNamespaceKeyword) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "namespace", "namespase");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_TRUE(catalogue->namespaces.empty());
}

TEST(XsmpTest, GenericRecoveryLeavesPartialAstForExtraKeywordCharacter) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "implements", "implementss");

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  ASSERT_TRUE(parsed.value) << "diag_count=" << parsed.parseDiagnostics.size()
                            << " size=" << input.size();
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  auto *catalogue = pegium::ast_ptr_cast<Xsmp::Catalogue>(parsed.value);
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_TRUE(catalogue->namespaces.empty());
}

TEST(XsmpTest,
     GenericRecoveryReportsDiagnosticsAfterSupportedMultiEditKeywordAndFurtherErrors) {
  class MultiErrorXsmpParser final : public Xsmp::XsmpParser {
  protected:
    pegium::parser::ParseOptions getParseOptions() const noexcept override {
      auto options = Xsmp::XsmpParser::getParseOptions();
      options.recoveryWindowTokenCount = 16;
      options.maxRecoveryWindowTokenCount = 64;
      options.maxRecoveryAttempts = 128;
      options.maxRecoveryEditsPerAttempt = 32;
      options.maxRecoveryEditCost = 256;
      options.maxRecoveryWindows = 8;
      return options;
    }
  } parser;

  const std::string input = R"(
    /* multi-edit typo on catalogue keyword */
    catalogxoe Demo 
    
    aaa /* extra tokens */ 

    namespace Core
    { 
      primitive Bool
      integer Int32 extends Smp.Int32
      @Meta( /*missing closing bracket */
      attribute Bool Flag = true
      Public abstract service MyService extends Base.Service implements IOne, ITwo
      {
        reference Bool refValue
        container Bool bag
        public constant Bool enabled = true
        input output field Bool flag = false
      }
      enum Mode
      {
        ON = true, //missing comma
        OFF = false
      }
    // missing closing square bracket for namespace
  )";

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  EXPECT_FALSE(parsed.fullMatch);
}

TEST(XsmpTest,
     GenericRecoveryStillStopsOnKeywordErrorBeyondFuzzyEnvelopeAndFurtherErrors) {
  class MultiErrorXsmpParser final : public Xsmp::XsmpParser {
  protected:
    pegium::parser::ParseOptions getParseOptions() const noexcept override {
      auto options = Xsmp::XsmpParser::getParseOptions();
      options.recoveryWindowTokenCount = 16;
      options.maxRecoveryWindowTokenCount = 64;
      options.maxRecoveryAttempts = 128;
      options.maxRecoveryEditsPerAttempt = 32;
      options.maxRecoveryEditCost = 256;
      options.maxRecoveryWindows = 8;
      return options;
    }
  } parser;

  const std::string input = R"(
    /* typo beyond fuzzy envelope on catalogue keyword */
    cxtxlgxxe Demo 

    aaa /* extra tokens */ 

    namespace Core
    { 
      primitive Bool
      integer Int32 extends Smp.Int32
      @Meta( /*missing closing bracket */
      attribute Bool Flag = true
      Public abstract service MyService extends Base.Service implements IOne, ITwo
      {
        reference Bool refValue
        container Bool bag
        public constant Bool enabled = true
        input output field Bool flag = false
      }
      enum Mode
      {
        ON = true, //missing comma
        OFF = false
      }
    // missing closing square bracket for namespace
  )";

  const auto document = parse_text(parser, input);
  auto &parsed = document->parseResult;
  EXPECT_FALSE(parsed.parseDiagnostics.empty());
  EXPECT_FALSE(parsed.fullMatch);
  EXPECT_FALSE(parsed.value);
}

TEST(XsmpBenchmark, ParseSpeedMicroBenchmark) {
  Xsmp::XsmpParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_XSMP_REPETITIONS", 1'500, /*minValue*/ 1);
  const auto payload = makeXsmpPayload(static_cast<std::size_t>(repetitions));
  const auto probe = parse_text(parser, payload);
  printParseReport("xsmp", probe->parseResult);

  const auto stats = pegium::test::runParseBenchmark(
      "xsmp", payload,
      [&](std::string_view text) { return parse_text(parser, text); });
  pegium::test::assertMinThroughput("xsmp", stats.mib_per_s);
}

TEST(XsmpBenchmark, ParseSpeedMicroBenchmarkCatalogueTypo) {
  Xsmp::XsmpParser parser;
  const auto repetitions =
      pegium::test::getEnvInt("PEGIUM_BENCH_XSMP_REPETITIONS", 1'500, 1);
  const auto payload = makeXsmpPayload(static_cast<std::size_t>(repetitions));
  auto typoPayload = replaceOnce(payload, "catalogue test", "catalogoe test");
  const auto probe = parse_text(parser, typoPayload);
  printParseReport(kXsmpTypoCatalogueBenchmarkName, probe->parseResult);

  const auto stats = pegium::test::runParseBenchmark(
      kXsmpTypoCatalogueBenchmarkName, typoPayload,
      [&](std::string_view text) { return parse_text(parser, text); });
  pegium::test::assertMinThroughput(kXsmpTypoCatalogueGateKey,
                                    stats.mib_per_s);
}
