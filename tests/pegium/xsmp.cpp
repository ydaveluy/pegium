#include <chrono>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/Parser.hpp>

namespace Xsmp {
struct Type;
struct Attribute : public pegium::AstNode {
  reference<Type> type;
};
struct NamedElement : public pegium::AstNode {
  string name;
  vector<containment<Attribute>> attributes;
};
struct VisibilityElement : public NamedElement {
  vector<string> modifiers;
};
struct Namespace;
struct Catalogue : public NamedElement {
  vector<containment<Namespace>> namespaces;
};

struct Namespace : public NamedElement {
  vector<containment<NamedElement>> members;
};

struct Type : public VisibilityElement {};
struct Structure : public Type {
  vector<containment<NamedElement>> members;
};

struct Class : public Structure {};

class XsmpParser : public pegium::Parser {
public:
  XsmpParser() {

    using namespace pegium::grammar;
    terminal("WS").ignore()(+s);
    terminal("SL_COMMENT").hide()("//"_kw >> &(eol | eof));
    terminal("ML_COMMENT").hide()("/*"_kw >> "*/"_kw);
    terminal("ID")("a-zA-Z_"_cr, *w);
    // rule("test")(ID,ID);
    rule("QualifiedName")(at_least_one_sep("."_kw, call("ID")));

    rule<Attribute>("Attribute")(
        "@"_kw, assign<&Attribute::type>(call("QualifiedName")),
        opt("("_kw, ")"_kw));
    rule<std::string>("Visibility")("private"_kw | "protected"_kw |
                                    "public"_kw);

    auto Attributes = *append<&NamedElement::attributes>(call("Attribute"));
    auto Name = assign<&NamedElement::name>(call("ID"));
    auto Visibilities =
        *append<&VisibilityElement::modifiers>(call("Visibility"));

    rule<Structure>("Structure")(
        Attributes, Visibilities, "struct"_kw, Name, "{"_kw,
        // many(&Structure::members +=       call("Constant") | call("Field")),
        "}"_kw);

    rule<Class>("Class")(
        Attributes,
        *append<&VisibilityElement::modifiers>(call("Visibility") |
                                               "abstract"_kw),
        "class"_kw, Name, "{"_kw,
        // many(&Structure::members += call("Constant") |       call("Field")),
        "}"_kw);

    rule<Type>("Type")(call("Structure") | call("Class"));
    rule<Namespace>("Namespace")(
        Attributes, "namespace"_kw, Name, "{"_kw,
        *append<&Namespace::members>(call("Namespace") | call("Type")), "}"_kw);

    rule<Catalogue>("Catalogue")(
        Attributes, "catalogue"_kw, Name,
        *append<&Catalogue::namespaces>(call("Namespace")));
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
      public protected private struct MyStruct{}

      private public abstract public class MyClass{}
    }
    namespace B
    {
    }
  )";
  for (std::size_t i = 0; i < 100'000; ++i) {
    input += R"(    
    namespace A
    {
      @Abstract()
      public protected private struct MyStruct{}
      @Abstract()
      private public abstract public class MyClass{}
    }
    namespace B
    {
    }
    /* a comment multi line*/
    )";
  }

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = g.parse("Catalogue", input);
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "Parsed " << result.len << " / " << input.size()
            << " characters in " << duration << "ms\n"  <<  ((1000*double(result.len)/ duration)/1'000'000)<<"MB/s\n";

  EXPECT_TRUE(result.ret);
}
