#include "xsmp.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/Parser.hpp>

namespace Xsmp2 {
using namespace Xsmp;
class XsmpParser : public pegium::Parser {
public:
#include <pegium/rule_macros_begin.h>
  TERM(WS);
  TERM(ID);
  TERM(ML_COMMENT);
  TERM(SL_COMMENT);
  TERM(Visibility);
  RULE(QualifiedName);
  RULE(Attribute, Xsmp::Attribute);
  RULE(Catalogue, Xsmp::Catalogue);
  RULE(Namespace, Xsmp::Namespace);
  RULE(NamespaceWithAttributes, Xsmp::Namespace);
  RULE(NamespaceMember, Xsmp::NamedElement);
  RULE(Structure, Xsmp::Structure);
  RULE(StructureMember, Xsmp::NamedElement);
  RULE(Interface, Xsmp::Interface);
  RULE(InterfaceMember, Xsmp::NamedElement);
  RULE(Class, Xsmp::Class);
  RULE(ClassMember, Xsmp::NamedElement);
  RULE(Exception, Xsmp::Exception);
  RULE(Model, Xsmp::Model);
  RULE(Service, Xsmp::Service);
  RULE(ComponentMember, Xsmp::NamedElement);
  RULE(ValueReference, Xsmp::ValueReference);
  RULE(Array, Xsmp::Array);
  RULE(StringType, Xsmp::StringType);
  RULE(PrimitiveType, Xsmp::PrimitiveType);
  RULE(NativeType, Xsmp::NativeType);
  RULE(AttributeType, Xsmp::AttributeType);
  RULE(Enumeration, Xsmp::Enumeration);
  RULE(EnumerationLiteral, Xsmp::EnumerationLiteral);
  RULE(Float, Xsmp::Float);
  RULE(Integer, Xsmp::Integer);
  RULE(EventType, Xsmp::EventType);
  RULE(Constant, Xsmp::Constant);
  RULE(Field, Xsmp::Field);
  RULE(Property, Xsmp::Property);
  RULE(Association, Xsmp::Association);
  RULE(Container, Xsmp::Container);
  RULE(Reference, Xsmp::Reference);
  RULE(Expression, Xsmp::Expression);
  RULE(OrExpression, Xsmp::Expression);
  RULE(AndExpression, Xsmp::Expression);
  RULE(BitwiseOrExpression, Xsmp::Expression);
  RULE(BitwiseXorExpression, Xsmp::Expression);
  RULE(BitwiseAndExpression, Xsmp::Expression);
  RULE(EqualityExpression, Xsmp::Expression);
  RULE(RelationalExpression, Xsmp::Expression);
  RULE(BitwiseExpression, Xsmp::Expression);
  RULE(AdditiveExpression, Xsmp::Expression);
  RULE(MultiplicativeExpression, Xsmp::Expression);
  RULE(UnaryOperation, Xsmp::Expression);
  RULE(BooleanLiteral, Xsmp::BooleanLiteral);
#include <pegium/rule_macros_end.h>

  XsmpParser() {
    using namespace pegium::grammar;
    WS = at_least_one(s);
    SL_COMMENT = "//"_kw <=> ~(eol | eof);
    ML_COMMENT = "/*"_kw <=> "*/"_kw;

    ID = "a-zA-Z_"_cr + many(w);

    QualifiedName = at_least_one_sep(ID, "."_kw);

    Visibility = "private"_kw | "protected"_kw | "public"_kw;

    Attribute =
        "@"_kw + assign<&Attribute::type>(QualifiedName) + opt("("_kw + ")"_kw);

    Catalogue = many(assign<&NamedElement::attributes>(Attribute)) +
                "catalogue"_kw + assign<&NamedElement::name>(ID) +
                many(assign<&Catalogue::namespaces>(Namespace));

    Namespace = many(assign<&NamedElement::attributes>(Attribute)) +
                "namespace"_kw + assign<&NamedElement::name>(QualifiedName) + //
                "{"_kw +                                                      //
                many(assign<&Namespace::members>(
                    Structure | Interface | Array | ValueReference | Float |
                    Integer | EventType | StringType | PrimitiveType |
                    NativeType | AttributeType | Enumeration | Class |
                    Exception | Model | Service)) + //
                "}"_kw;                             //

    Enumeration =
        many(assign<&NamedElement::attributes>(Attribute)) +
        many(assign<&VisibilityElement::modifiers>(Visibility)) + "enum"_kw +
        assign<&NamedElement::name>(ID) + "{"_kw +
        many_sep(assign<&Enumeration::literals>(EnumerationLiteral), ","_kw) +
        "}"_kw;

    EnumerationLiteral = assign<&NamedElement::name>(ID) + "="_kw +
                         assign<&EnumerationLiteral::value>(Expression);

    Structure = many(assign<&NamedElement::attributes>(Attribute)) +
                many(assign<&VisibilityElement::modifiers>(Visibility)) +
                "struct"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
                many(assign<&Structure::members>(Field | Constant)) + "}"_kw;

    Class = many(assign<&NamedElement::attributes>(Attribute)) +
            many(assign<&VisibilityElement::modifiers>(Visibility |
                                                       "abstract"_kw)) +
            "class"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
            many(assign<&Structure::members>(Field | Constant | Property |
                                             Association)) +
            "}"_kw;

    Exception = many(assign<&NamedElement::attributes>(Attribute)) +
                many(assign<&VisibilityElement::modifiers>(Visibility |
                                                           "abstract"_kw)) +
                "exception"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
                many(assign<&Structure::members>(Field | Constant | Property |
                                                 Association)) +
                "}"_kw;

    Interface = many(assign<&NamedElement::attributes>(Attribute)) +
                many(assign<&VisibilityElement::modifiers>(Visibility)) +
                "interface"_kw + assign<&NamedElement::name>(ID) +
                // base interfaces
                opt("extends"_kw +
                    at_least_one_sep(assign<&Interface::bases>(QualifiedName),
                                     ","_kw)) +
                "{"_kw +
                many(assign<&Structure::members>(Constant | Property)) + "}"_kw;

    Model =
        many(assign<&NamedElement::attributes>(Attribute)) +
        many(
            assign<&VisibilityElement::modifiers>(Visibility | "abstract"_kw)) +
        "model"_kw + assign<&NamedElement::name>(ID) +
        // base class
        opt("extends"_kw + assign<&Component::base>(QualifiedName)) +
        // base interfaces
        opt("implements"_kw +
            at_least_one_sep(assign<&Component::interfaces>(QualifiedName),
                             ","_kw)) +
        "{"_kw +
        many(assign<&Component::members>(Constant | Association | Field |
                                         Property | Container | Reference)) +
        "}"_kw;
    Service =
        many(assign<&NamedElement::attributes>(Attribute)) +
        many(
            assign<&VisibilityElement::modifiers>(Visibility | "abstract"_kw)) +
        "service"_kw + assign<&NamedElement::name>(ID) +
        // base class
        opt("extends"_kw + assign<&Component::base>(QualifiedName)) +
        // base interfaces
        opt("implements"_kw +
            at_least_one_sep(assign<&Component::interfaces>(QualifiedName),
                             ","_kw)) +
        "{"_kw +
        many(assign<&Component::members>(Constant | Association | Field |
                                         Property | Container | Reference)) +
        "}"_kw;

    Array = many(assign<&NamedElement::attributes>(Attribute)) +
            many(assign<&VisibilityElement::modifiers>(Visibility)) +
            "array"_kw + assign<&NamedElement::name>(ID) + "="_kw +
            assign<&Array::itemType>(QualifiedName) + "["_kw +
            assign<&Array::size>(Expression) + "]"_kw;

    StringType = many(assign<&NamedElement::attributes>(Attribute)) +
                 many(assign<&VisibilityElement::modifiers>(Visibility)) +
                 "string"_kw + assign<&NamedElement::name>(ID) + "="_kw +
                 "["_kw + assign<&StringType::size>(Expression) + "]"_kw;

    ValueReference = many(assign<&NamedElement::attributes>(Attribute)) +
                     many(assign<&VisibilityElement::modifiers>(Visibility)) +
                     "using"_kw + assign<&NamedElement::name>(ID) + "="_kw +
                     assign<&ValueReference::type>(QualifiedName) + "*"_kw;

    Float = many(assign<&NamedElement::attributes>(Attribute)) +
            many(assign<&VisibilityElement::modifiers>(Visibility)) +
            "float"_kw + assign<&NamedElement::name>(ID) +
            opt("extends"_kw + assign<&Float::primitiveType>(QualifiedName))
        // TODO add min/max
        ;
    Integer = many(assign<&NamedElement::attributes>(Attribute)) +
              many(assign<&VisibilityElement::modifiers>(Visibility)) +
              "integer"_kw + assign<&NamedElement::name>(ID) +
              opt("extends"_kw + assign<&Integer::primitiveType>(QualifiedName))
        // TODO add min/max
        ;
    EventType = many(assign<&NamedElement::attributes>(Attribute)) +
                many(assign<&VisibilityElement::modifiers>(Visibility)) +
                "event"_kw + assign<&NamedElement::name>(ID) +
                opt("extends"_kw + assign<&EventType::eventArg>(QualifiedName));
    PrimitiveType = many(assign<&NamedElement::attributes>(Attribute)) +
                    many(assign<&VisibilityElement::modifiers>(Visibility)) +
                    "primitive"_kw + assign<&NamedElement::name>(ID);
    NativeType = many(assign<&NamedElement::attributes>(Attribute)) +
                 many(assign<&VisibilityElement::modifiers>(Visibility)) +
                 "native"_kw + assign<&NamedElement::name>(ID);

    AttributeType = many(assign<&NamedElement::attributes>(Attribute)) +
                    many(assign<&VisibilityElement::modifiers>(Visibility)) +
                    "attribute"_kw +
                    assign<&AttributeType::type>(QualifiedName) +
                    assign<&NamedElement::name>(ID) +
                    opt("="_kw + assign<&AttributeType::value>(Expression));
    Constant = many(assign<&NamedElement::attributes>(Attribute)) +
               many(assign<&VisibilityElement::modifiers>(Visibility)) +
               "constant"_kw + assign<&Constant::type>(QualifiedName) +
               assign<&NamedElement::name>(ID) + "="_kw +
               assign<&Constant::value>(Expression);

    Field = many(assign<&NamedElement::attributes>(Attribute)) +
            many(assign<&VisibilityElement::modifiers>(
                Visibility | "input"_kw | "output"_kw | "transient"_kw)) +

            "field"_kw + assign<&Field::type>(QualifiedName) +
            assign<&NamedElement::name>(ID) +
            opt("="_kw + assign<&Field::value>(Expression));

    Property =
        many(assign<&NamedElement::attributes>(Attribute)) +
        many(assign<&VisibilityElement::modifiers>(
            Visibility | "readWrite"_kw | "readOnly"_kw | "writeOnly"_kw)) +

        "property"_kw + assign<&Property::type>(QualifiedName) +
        assign<&NamedElement::name>(ID);

    Association = many(assign<&NamedElement::attributes>(Attribute)) +
                  many(assign<&VisibilityElement::modifiers>(Visibility)) +
                  "association"_kw + assign<&Association::type>(QualifiedName) +
                  assign<&NamedElement::name>(ID);

    Container = "container"_kw + assign<&Container::type>(QualifiedName) +
                assign<&NamedElement::name>(ID);

    Reference = "reference"_kw + assign<&Reference::type>(QualifiedName) +
                assign<&NamedElement::name>(ID);

    Expression = OrExpression;

    OrExpression = AndExpression +
                   many(action<&BinaryExpression::leftOperand>() +
                        assign<&BinaryExpression::feature>("||"_kw) +
                        assign<&BinaryExpression::rightOperand>(AndExpression));

    AndExpression =
        BitwiseOrExpression +
        many(action<&BinaryExpression::leftOperand>() +
             assign<&BinaryExpression::feature>("&&"_kw) +
             assign<&BinaryExpression::rightOperand>(BitwiseOrExpression));

    BitwiseOrExpression = BooleanLiteral;

    BooleanLiteral = assign<&BooleanLiteral::isTrue>("true"_kw) | "false"_kw;
  }
  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};

} // namespace Xsmp2

TEST(XsmpTest, TestCatalogue) {
  Xsmp2::XsmpParser g;

  std::string input = R"(
    /**
     * A demo catalogue
     */
    catalogue test 

  )";
  for (std::size_t i = 0; i < 200'000; ++i) {
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

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = g.Catalogue.parse(input, g.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "XSMP2 Parsed " << result.len / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  ASSERT_TRUE(result.ret);

  EXPECT_EQ(result.value->name, "test");
  // EXPECT_EQ(result.value->namespaces.size(), 400'002);
}
