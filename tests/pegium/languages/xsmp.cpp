#include "xsmp.hpp"
#include <algorithm>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/Parser.hpp>
#include <sstream>
#include <stdexcept>

namespace Xsmp {

using namespace pegium::parser;
class XsmpParser : public Parser {
public:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"

XsmpParser(){
    WS=WS.super();
}

  Terminal<> WS{"WS", some(s)};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

  Terminal<> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<> QualifiedName{"QualifiedName", some(ID, "."_kw)};

  //Rule<> Visibility{"Visibility", "private"_kw | "protected"_kw | "public"_kw};
  static constexpr auto Visibility = "private"_kw | "protected"_kw | "public"_kw;

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

  Rule<Xsmp::Expression> Expression{"Expression", OrExpression};

  Rule<Xsmp::Expression> OrExpression{
      "OrExpression",
      AndExpression +
          many(action<&BinaryExpression::leftOperand>() +
               assign<&BinaryExpression::feature>("||"_kw) +
               assign<&BinaryExpression::rightOperand>(AndExpression))};

  Rule<Xsmp::Expression> AndExpression{
      "AndExpression",
      BitwiseOrExpression +
          many(action<&BinaryExpression::leftOperand>() +
               assign<&BinaryExpression::feature>("&&"_kw) +
               assign<&BinaryExpression::rightOperand>(BitwiseOrExpression))};

  Rule<Xsmp::Expression> BitwiseOrExpression{"BitwiseOrExpression",
                                             BooleanLiteral};

  Rule<Xsmp::BooleanLiteral> BooleanLiteral{
      "BooleanLiteral",
      enable_if<&BooleanLiteral::isTrue>("true"_kw) | "false"_kw};

#pragma clang diagnostic pop
  auto createContext() const {
    return SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};
} // namespace Xsmp

namespace {

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

void assertCatalogueAst(const std::shared_ptr<Xsmp::Catalogue> &catalogue,
                        bool allowMissingNamespaceName = false) {
  ASSERT_TRUE(catalogue != nullptr);
  EXPECT_EQ(catalogue->name, "Demo");
  ASSERT_EQ(catalogue->namespaces.size(), 1u);
  ASSERT_TRUE(catalogue->namespaces[0] != nullptr);

  const auto &ns = catalogue->namespaces[0];
  if (allowMissingNamespaceName) {
    EXPECT_TRUE(ns->name.empty() || ns->name == "Core");
  } else {
    EXPECT_EQ(ns->name, "Core");
  }
  ASSERT_EQ(ns->members.size(), 5u);

  const auto *primitive = dynamic_cast<const Xsmp::PrimitiveType *>(
      ns->members[0].get());
  ASSERT_NE(primitive, nullptr);
  EXPECT_EQ(primitive->name, "Bool");

  const auto *integer = dynamic_cast<const Xsmp::Integer *>(ns->members[1].get());
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

  const auto service = std::dynamic_pointer_cast<Xsmp::Service>(ns->members[3]);
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

  const auto *field = dynamic_cast<const Xsmp::Field *>(service->members[3].get());
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
  ASSERT_TRUE(enumeration->literals[1] != nullptr);
  EXPECT_EQ(enumeration->literals[0]->name, "ON");
  EXPECT_EQ(enumeration->literals[1]->name, "OFF");
  ASSERT_TRUE(enumeration->literals[0]->value != nullptr);
  ASSERT_TRUE(enumeration->literals[1]->value != nullptr);
  const auto *onValue = asBoolLiteral(enumeration->literals[0]->value.get());
  const auto *offValue = asBoolLiteral(enumeration->literals[1]->value.get());
  ASSERT_NE(onValue, nullptr);
  ASSERT_NE(offValue, nullptr);
  EXPECT_TRUE(onValue->isTrue);
  EXPECT_FALSE(offValue->isTrue);
}

template <typename T>
void expectDiagnosticKind(
    const pegium::parser::ParseResult<T> &parsed,
    pegium::parser::ParseDiagnosticKind kind) {
  const bool hasKind =
      std::any_of(parsed.diagnostics.begin(), parsed.diagnostics.end(),
                  [kind](const pegium::parser::ParseDiagnostic &diag) {
                    return diag.kind == kind;
                  });
  if (!hasKind) {
    std::ostringstream oss;
    bool first = true;
    for (const auto &diag : parsed.diagnostics) {
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

} // namespace

TEST(XsmpTest, ParseBuildsExpectedAstForSmallCatalogue) {
  Xsmp::XsmpParser parser;

  const std::string input = makeCatalogueReferenceText();

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  EXPECT_FALSE(parsed.recovered);
  ASSERT_TRUE(parsed.ret) << "recovered=" << parsed.recovered
                          << " len=" << parsed.len
                          << " size=" << input.size();
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoverySkipsUnexpectedTokensAndBuildsAst) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "catalogue", "catalogoe");

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  expectDiagnosticKind(parsed, pegium::parser::ParseDiagnosticKind::Replaced);
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoverySkipsUnexpectedTokenInMiddleOfServiceHeader) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "implements", "implxments");

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  expectDiagnosticKind(parsed, pegium::parser::ParseDiagnosticKind::Replaced);
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoverySkipsUnexpectedTokensInMiddleOfServiceBody) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "constant", "constxnt");

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  expectDiagnosticKind(parsed, pegium::parser::ParseDiagnosticKind::Replaced);
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoveryFixesTypoInNamespaceKeyword) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "namespace", "namespase");

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  expectDiagnosticKind(parsed, pegium::parser::ParseDiagnosticKind::Replaced);
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoveryDeletesExtraCharacterAfterKeyword) {
  Xsmp::XsmpParser parser;

  const std::string input =
      replaceOnce(makeCatalogueReferenceText(), "implements", "implementss");

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  const bool hasDeleteOrReplace =
      std::any_of(parsed.diagnostics.begin(), parsed.diagnostics.end(),
                  [](const pegium::parser::ParseDiagnostic &diag) {
                    return diag.kind == pegium::parser::ParseDiagnosticKind::Deleted ||
                           diag.kind == pegium::parser::ParseDiagnosticKind::Replaced;
                  });
  EXPECT_TRUE(hasDeleteOrReplace);
  assertCatalogueAst(parsed.value);
}

TEST(XsmpTest, RecoveryMultipleErrors) {
  Xsmp::XsmpParser parser;

  const std::string input =R"(
    /* typo on catalogue keyword */
    cataloge Demo 
    
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
        ON = true //missing comma
        OFF = false
      }
    // missing closing square bracket for namespace
  )";

  const auto parsed = parser.Catalogue.parse(input, parser.createContext());
  ASSERT_TRUE(parsed.ret) << "len=" << parsed.len << " size=" << input.size();
  EXPECT_TRUE(parsed.recovered);
  EXPECT_FALSE(parsed.diagnostics.empty());
  //expectDiagnosticKind(parsed, pegium::parser::ParseDiagnosticKind::Replaced);
  assertCatalogueAst(parsed.value, /*allowMissingNamespaceName=*/true);
}


TEST(XsmpBenchmark, ParseSpeedMicroBenchmark) {
  Xsmp::XsmpParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_XSMP_REPETITIONS", 1'500, /*minValue*/ 1);
  const auto payload = makeXsmpPayload(static_cast<std::size_t>(repetitions));

  const auto stats = pegium::test::runParseBenchmark(
      "xsmp", payload, [&](std::string_view text) {
        return parser.Catalogue.parse(text, parser.createContext());
      });
  pegium::test::assertMinThroughput("xsmp", stats.mib_per_s);
}
