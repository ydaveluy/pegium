#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/Parser.hpp>

namespace Xsmp {
struct Type;
struct Attribute : public pegium::AstNode {
  /*reference<Type>*/ string type;
};
struct NamedElement : public pegium::AstNode {
  string name;
  vector<pointer<Attribute>> attributes;
};

struct VisibilityElement : public NamedElement {
  vector<string> modifiers;
};
struct Namespace;
struct Catalogue : public NamedElement {
  vector<pointer<Namespace>> namespaces;
};

struct Namespace : public NamedElement {
  vector<pointer<NamedElement>> members;
};

struct Type : public VisibilityElement {};
struct Structure : public Type {
  vector<pointer<NamedElement>> members;
};

struct Class : public Structure {};

struct Expression : pegium::AstNode {};
struct BinaryExpression : Expression {
  pointer<Expression> leftOperand;
  string feature;
  pointer<Expression> rightOperand;
};

struct BooleanLiteral : Expression {
  bool isTrue;
};
struct Constant : public VisibilityElement {
  string type;
  pointer<Expression> value;
};

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
  RULE(NamespaceMember, std::shared_ptr<Xsmp::NamedElement>);
  RULE(Type, std::shared_ptr<Xsmp::Type>);
  RULE(Structure, Xsmp::Structure);
  RULE(StructureMember, std::shared_ptr<Xsmp::NamedElement>);
  RULE(Constant, Xsmp::Constant);
  RULE(Expression, std::shared_ptr<Xsmp::Expression>);
  RULE(OrExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(AndExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(BitwiseOrExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(BitwiseXorExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(BitwiseAndExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(EqualityExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(RelationalExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(BitwiseExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(AdditiveExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(MultiplicativeExpression, std::shared_ptr<Xsmp::Expression>);
  RULE(UnaryOperation, std::shared_ptr<Xsmp::Expression>);
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
                "namespace"_kw + assign<&NamedElement::name>(QualifiedName) +
                "{"_kw + many(assign<&Namespace::members>(NamespaceMember)) +
                "}"_kw;

    NamespaceMember = Namespace | Type;
    Type = Structure;
    Structure = many(assign<&NamedElement::attributes>(Attribute)) +
                many(assign<&VisibilityElement::modifiers>(Visibility)) +
                "struct"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
                many(assign<&Structure::members>(StructureMember)) + "}"_kw;
    StructureMember = Constant;
    Constant = many(assign<&NamedElement::attributes>(Attribute)) +
               many(assign<&VisibilityElement::modifiers>(Visibility)) +
               "constant"_kw + assign<&Constant::type>(QualifiedName) +
               assign<&NamedElement::name>(ID) + "="_kw +
               assign<&Constant::value>(Expression);

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
    /*auto Attributes = *append<&NamedElement::attributes>(attribute);
    auto Name = assign<&NamedElement::name>(ID);
    auto Visibilities = *append<&VisibilityElement::modifiers>(visibility);

    auto structure = rule<Structure>("Structure") =
        Attributes + Visibilities + "struct"_kw + Name + "{"_kw +
        // many(&Structure::members +=       call("Constant") | call("Field")),
        "}"_kw;

    auto class_ = rule<Class>("Class") =
        Attributes +
        *append<&VisibilityElement::modifiers>(visibility | "abstract"_kw) +
        "class"_kw + Name + "{"_kw +
        // many(&Structure::members += call("Constant") |       call("Field")),
        "}"_kw;

    auto type = rule<Type>("Type") = structure | class_;
    auto ns = rule<Namespace>("Namespace") =
        Attributes + "namespace"_kw + Name + "{"_kw +
        *append<&Namespace::members>(call("Namespace") | type) + "}"_kw;

    rule<Catalogue>("Catalogue") = Attributes + "catalogue"_kw + Name +
                                   *(append<&Catalogue::namespaces>(ns));*/
  }
  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};

} // namespace Xsmp

TEST(XsmpTest, TestCatalogue) {
  Xsmp::XsmpParser g;

  std::string input = R"(
    /**
     * A demo catalogue
     */
    catalogue test 
    // a single line comment
    namespace A
    {
      @Abstract()
      public protected private struct MyStruct{
      }

      private public  public struct MyClass{}
    }
    namespace B
    {
    }
  )";
  for (std::size_t i = 0; i < 200'000; ++i) {
    input += R"(    
    namespace A
    {
      @Abstract()
      public protected private struct MyStruct{
        constant Int8 c1 = true
        public constant Int8 c1 = false
        constant Int8 c1 = false //|| true
        protected constant Int8 c1 = false
        constant Int8 c1 = false
        constant Int8 c1 = false
       private constant Int8 c1 = false
        constant Int8 c1 = false
        constant Int8 c1 = false
      
      }
      @Abstract()
      private public  public struct MyClass{}
    }
    namespace B
    {
    }
    /* a comment multi line */
    )";
  }

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = g.Catalogue.parse(input, g.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "XSMP Parsed " << input.size() / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  EXPECT_TRUE(result.ret);

  EXPECT_EQ(result.value.name, "test");
}
