#include "xsmp.hpp"
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/Parser.hpp>

using namespace pegium::grammar;
namespace Xsmp {
class XsmpRecoveryParser : public pegium::Parser {
public:
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<> WS{"WS", at_least_one(s)};
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> ~(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Terminal<> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<> QualifiedName{"QualifiedName", at_least_one_sep(ID, "."_kw)};
  Rule<Xsmp::Attribute> Attribute{
      "Attribute",
      "@"_kw + assign<&Attribute::type>(QualifiedName) + opt("("_kw + ")"_kw)};

  Rule<Xsmp::Catalogue> Catalogue{
      "Catalogue",
      many(assign<&NamedElement::attributes>(Attribute)) + "catalogue"_kw +
          assign<&NamedElement::name>(ID) +
          many(assign<&Catalogue::namespaces>(NamespaceWithAttributes))};

  Rule<Xsmp::Namespace> NamespaceWithAttributes{
      "NamespaceWithAttributes",
      many(assign<&NamedElement::attributes>(Attribute)) + Namespace};

  Rule<Xsmp::Namespace> Namespace{
      "Namespace", "namespace"_kw +
                       assign<&NamedElement::name>(QualifiedName) +         //
                       "{"_kw +                                             //
                       many(assign<&Namespace::members>(NamespaceMember)) + //
                       "}"_kw};                                             //

  Rule<Xsmp::NamedElement> NamespaceMember{
      "NamespaceMember",
      // attributes
      many(assign<&NamedElement::attributes>(Attribute)) +
          (
              // Namespace
              Namespace | Structure | Enumeration
              //
              )};
  Rule<Xsmp::Enumeration> Enumeration{
      "Enumeration",
      "enum"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
          many_sep(assign<&Enumeration::literals>(EnumerationLiteral), ","_kw) +
          "}"_kw};

  Rule<Xsmp::EnumerationLiteral> EnumerationLiteral{
      "EnumerationLiteral", assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Structure> Structure{
      "Structure", "struct"_kw + assign<&NamedElement::name>(ID) + "{"_kw +
                       many(assign<&Structure::members>(StructureMember)) +
                       "}"_kw};
  Rule<Xsmp::NamedElement> StructureMember{
      "StructureMember",
      many(assign<&NamedElement::attributes>(Attribute)) +
         // many(assign<&VisibilityElement::modifiers>(Visibility)) +
          (Field | Constant)};

  Rule<Xsmp::Constant> Constant{
      "Constant", "constant"_kw + assign<&Constant::type>(QualifiedName) +
                      assign<&NamedElement::name>(ID)};

  Rule<Xsmp::Field> Field{
      "Field",
      opt(assign<&VisibilityElement::modifiers>("input"_kw | "output"_kw |
                                                "transient"_kw)// +
         // many(assign<&VisibilityElement::modifiers>(
          //    Visibility | "input"_kw | "output"_kw | "transient"_kw))
          ) +
          "field"_kw + assign<&Field::type>(QualifiedName) +
          assign<&NamedElement::name>(ID)};

#pragma clang diagnostic pop
  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
  }
};

} // namespace Xsmp

TEST(Recovery, TestCatalogue) {
  Xsmp::XsmpRecoveryParser g;

  std::string input = R"(
    /**
     * A demo catalogue
     */
    catalogue test 

  )";

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = g.Catalogue.parse(input, g.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "XSMP Parsed " << result.len / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  ASSERT_TRUE(result.ret);

  EXPECT_EQ(result.value->name, "test");
  // EXPECT_EQ(result.value->namespaces.size(), 400'002);
}
