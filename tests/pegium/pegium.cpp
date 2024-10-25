#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
// #include <pegium/CstNodePrinter.hpp>
#include <fstream>
#include <pegium/Parser.hpp>

using namespace pegium;

struct TestAst : public pegium::AstNode {

  std::string name;
  std::string text;
  std::vector<std::shared_ptr<TestAst>> child;
};

class TestGrammar : public Parser {
public:
  TestGrammar() {

    using namespace pegium;
    ignored_terminal("WS", at_least_one(s()));
    hidden_terminal("SL_COMMENT", "//"_kw, many(cls("\r\n", true)));
    hidden_terminal("ML_COMMENT", "/*"_kw, many(!"*/"_kw, any()), "*/"_kw);
    terminal("ID", cls("a-zA-Z_"), many(w()));
    rule("QualifiedName", at_least_one_sep('.'_kw, call("ID")));

    rule<TestAst>(
        "TestAst", "test"_kw, &TestAst::name += call("ID"),
        opt("{"_kw, many(&TestAst::child += call("TestAst")), "}"_kw));
  }
};

struct Ref {
protected:
  Ref() = default;
};
TEST(PegiumTest, TestAst) {
  TestGrammar g;
  auto result =
      g.parse("TestAst", R"(
      test name   
      { 
        test child1
        test child2
        {
          test nested
        }
      }
      )"); 
  EXPECT_TRUE(result.ret);
  auto test = std::any_cast<std::shared_ptr<pegium::AstNode>>(result.value);
  auto *ast = dynamic_cast<TestAst *>(test.get());
  ASSERT_TRUE(ast);
  EXPECT_EQ(ast->name, "name");
  ASSERT_EQ(ast->child.size(), 2);
  EXPECT_EQ(ast->child[0]->name, "child1");
  EXPECT_EQ(ast->child[1]->name, "child2");

  ASSERT_EQ(ast->child[1]->child.size(), 1);
  EXPECT_EQ(ast->child[1]->child[0]->name, "nested");

}

TEST(PegiumTest, QualifiedName) {
  TestGrammar g;
  auto result = g.parse("QualifiedName", "a.b.c");
  EXPECT_TRUE(result.ret);
  auto str = std::any_cast<std::string>(result.value);
  EXPECT_EQ(str, "a.b.c");
}

TEST(PegiumTest, QualifiedNameWithSpacesAndComment) {
  TestGrammar g;
  auto result = g.parse("QualifiedName", R"(
  /**
   * multi line comment
   */
  a  .
  // single line comment
  b
  .
  
  c
  // trailing comment ->
  //)");
  EXPECT_TRUE(result.ret);
  auto str = std::any_cast<std::string>(result.value);
  EXPECT_EQ(str, "a.b.c");
  // pegium::CstNodeToJson::print(*result.node, std::cout);
}
TEST(PegiumTest, DISABLED_Bench) {
  TestGrammar g;

  std::string input;
  for (int i = 0; i < 1'000'000; ++i)
    input += R"(
    // comment
    a.b.c.d.e.
    /* comment*/
    g.h.
    )";
  input += "end";

  // std::ofstream out("output.txt");
  // out << input;

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto l = g.parse("QualifiedName", input);

  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "Parsed " << l.len << " / " << input.size() << " characters in "
            << duration << "ms\n";
}
