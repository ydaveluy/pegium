

#include <gtest/gtest.h>
#include <pegium/Parser.hpp>
#include <string>
#include <type_traits>

struct Attribute : pegium::AstNode {
  std::string type;
};

struct Attributes : pegium::AstNode {
  vector<std::shared_ptr<Attribute>> attributes;
};

struct MyParser : pegium::Parser {
#include <pegium/rule_macros_begin.h>
  TERM(WS);
  TERM(ID);
  TERM(ML_COMMENT);
  RULE(QualifiedName);
  RULE(Attribute, std::shared_ptr<::Attribute>);
  RULE(Attributes, ::Attributes);
#include <pegium/rule_macros_end.h>

  MyParser() {
    using namespace pegium::grammar;
    // setEntryRule(Attribute);
    WS = at_least_one(s);
    ID = "a-zA-Z_"_cr + many(w);
    ML_COMMENT = "/*"_kw <=> "*/"_kw;
    QualifiedName = at_least_one_sep(ID, "."_kw);
    Attribute = "@"_kw + assign<&Attribute::type>(QualifiedName) +
                opt("("_kw + opt(QualifiedName) + ")"_kw);

    Attributes = many(assign<&Attributes::attributes>(Attribute));
  }

  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().ignore(WS).hide(ML_COMMENT).build();
  }
};

MyParser p;
TEST(GrammarTest, Attribute) {
  auto result = p.Attribute.parse(" /* comment */ @ test /* aa   */.a.e",
                                  p.createContext());

  EXPECT_TRUE(result.ret);
  auto &attribute = result.value;
  //  ASSERT_TRUE(attribute);
  EXPECT_EQ(attribute->type, "test.a.e");
}

TEST(GrammarTest, Attributes) {

  auto result = p.Attributes.parse(" /* comment */ @ test /* aa   */.a.e",
                                   p.createContext());

  EXPECT_TRUE(result.ret);
  auto &attribute = result.value;
  //  ASSERT_TRUE(attribute);
  EXPECT_EQ(attribute.attributes.size(), 1);

  constexpr std::size_t SIZE = 5;
  std::string input;
  input.reserve(SIZE * 10);
  for (int i = 0; i < SIZE; ++i)
    input += "@text" + std::to_string(std::rand()) + ".aaa.bbb(aaa)\n";
  using namespace std::chrono;

  auto start = high_resolution_clock::now();

  auto i = p.Attributes.parse(input, p.createContext());
  // auto i = g.parse_rule(input , node,c);
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  EXPECT_TRUE(i.ret);
  std::cout << "Parsed " << input.size() / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * i.len / duration) / static_cast<double>(1024 * 1024))
            << " Mo/s\n";
}