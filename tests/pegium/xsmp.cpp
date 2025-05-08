#include "xsmp.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/parser/Parser.hpp>

namespace Xsmp {

using namespace pegium::parser;
class XsmpParser : public Parser {
public:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<> WS{"WS", some(s)};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

  Terminal<> ID{"ID", "a-zA-Z_"_cr + many(w)};

  Rule<> QualifiedName{"QualifiedName", some(ID, "."_kw)};

  Rule<> Visibility{"Visibility", "private"_kw | "protected"_kw | "public"_kw};

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
  std::unique_ptr<IContext> createContext() const override {
    return ContextBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};
} // namespace Xsmp

TEST(XsmpTest, TestCatalogue) {
  Xsmp::XsmpParser parser;

  std::string input = R"(
    /**
     * A demo catalogue
     */
    catalogue test 

  )";
  for (std::size_t i = 0; i < 20'000; ++i) {
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

  std::cout << parser.Catalogue << ": " << *parser.Catalogue.getElement()
            << std::endl;
  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = parser.Catalogue.parse(input, parser.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "XSMP Parsed " << result.len / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  EXPECT_TRUE(result.ret);
  ASSERT_TRUE(result.value);

  EXPECT_EQ(result.value->name, "test");

  // for (const auto *node : result.value->getAllContent<Xsmp::NamedElement>())
  //   std::cout << node->name << std::endl;

  // std::cout << "parsed " << result.value->namespaces.size() << "
  // namespace\n";
  //  EXPECT_EQ(result.value->namespaces.size(), 400'002);
}
