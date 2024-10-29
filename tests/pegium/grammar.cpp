
#include <gtest/gtest.h>

#include <pegium/Parser.hpp>

TEST(GrammarTest, Optional) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(opt("test"_kw));
      terminal("TERM")(opt("test"_kw));
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_FALSE(p.parse("RULE", "test test").ret);
  EXPECT_FALSE(p.parse("RULE", "testtest").ret);

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test ").ret);
  EXPECT_FALSE(p.parse("TERM", " test").ret);
  EXPECT_FALSE(p.parse("TERM", "testtest").ret);
}

TEST(GrammarTest, Many) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(many("test"_kw));
      terminal("TERM")(many("test"_kw));
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test test test").ret);

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttesttesttest").ret);
  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test ").ret);
  EXPECT_FALSE(p.parse("TERM", " test").ret);
  EXPECT_FALSE(p.parse("TERM", "testtest ").ret);
}

TEST(GrammarTest, ManySep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(many_sep('.'_kw, "test"_kw));
      terminal("TERM")(many_sep('.'_kw, "test"_kw));
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", ".").ret);
  EXPECT_FALSE(p.parse("RULE", "test.").ret);
  EXPECT_TRUE(p.parse("RULE", "").ret);

  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", " test . test ").ret);
  EXPECT_TRUE(p.parse("RULE", "test.test.test. test.test").ret);

  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test .").ret);
  EXPECT_FALSE(p.parse("TERM", " test.").ret);
  EXPECT_FALSE(p.parse("TERM", "test.test ").ret);

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test.test.test.test").ret);
}

TEST(GrammarTest, AtLeastOne) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(at_least_one("test"_kw));
      terminal("TERM")(at_least_one("test"_kw));
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_FALSE(p.parse("RULE", "testtest").ret);
  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test test test").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_FALSE(p.parse("TERM", "test test").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttesttesttest").ret);
}
TEST(GrammarTest, AtLeastOneSep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(at_least_one_sep('.'_kw, "test"_kw));
      terminal("TERM")(at_least_one_sep('.'_kw, "test"_kw));
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_FALSE(p.parse("RULE", ".").ret);
  EXPECT_FALSE(p.parse("RULE", "test.").ret);
  EXPECT_TRUE(p.parse("RULE", "test ").ret);
  EXPECT_TRUE(p.parse("RULE", "test .test").ret);
  EXPECT_TRUE(p.parse("RULE", "  test.test . test.test.test  ").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_FALSE(p.parse("TERM", ".").ret);
  EXPECT_FALSE(p.parse("TERM", "test.").ret);
  EXPECT_FALSE(p.parse("TERM", "test .test").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test.test.test.test").ret);
}

TEST(GrammarTest, Repetition) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(rep(2, 3, "test"_kw));
      terminal("TERM")(rep(2, 3, "test"_kw));
    }
  };
  Parser p;
  EXPECT_FALSE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test").ret);
  EXPECT_FALSE(p.parse("RULE", "test test test test").ret);

  EXPECT_FALSE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttest").ret);
  EXPECT_FALSE(p.parse("TERM", "testtesttesttest").ret);
}

TEST(GrammarTest, Group) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")("A"_kw , "B"_kw);
      terminal("TERM")("A"_kw , "B"_kw);
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  B").ret);
  EXPECT_FALSE(p.parse("RULE", "A ").ret);

  EXPECT_FALSE(p.parse("TERM", "A").ret);
  EXPECT_TRUE(p.parse("TERM", "AB").ret);
  EXPECT_FALSE(p.parse("TERM", " AB").ret);
}


TEST(GrammarTest, UnorderedGroup) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")("A"_kw & "B"_kw & "C"_kw);
      terminal("TERM")("A"_kw & "B"_kw & "C"_kw);
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", "  A  B C").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  C B").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  A C").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  C A").ret);
  EXPECT_TRUE(p.parse("RULE", "  C  A B").ret);
  EXPECT_TRUE(p.parse("RULE", "  C  B A").ret);

  EXPECT_FALSE(p.parse("RULE", "A B B").ret);
  EXPECT_FALSE(p.parse("RULE", "A C").ret);

  EXPECT_TRUE(p.parse("TERM", "ABC").ret);
  EXPECT_TRUE(p.parse("TERM", "ACB").ret);
  EXPECT_TRUE(p.parse("TERM", "BAC").ret);
  EXPECT_TRUE(p.parse("TERM", "BCA").ret);
  EXPECT_TRUE(p.parse("TERM", "CAB").ret);
  EXPECT_TRUE(p.parse("TERM", "CBA").ret);

  EXPECT_FALSE(p.parse("TERM", "ABB").ret);
  EXPECT_FALSE(p.parse("TERM", "AC").ret);
}

TEST(GrammarTest, PrioritizedChoice) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")("A"_kw | "B"_kw);
      terminal("TERM")("A"_kw | "B"_kw);
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  ").ret);
  EXPECT_FALSE(p.parse("RULE", "A B").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "A").ret);
  EXPECT_TRUE(p.parse("TERM", "B").ret);
  EXPECT_FALSE(p.parse("TERM", " A").ret);
  EXPECT_FALSE(p.parse("TERM", "A ").ret);
}

TEST(GrammarTest, PrioritizedChoiceWithGroup) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s);
      rule("RULE")(("A"_kw , "B"_kw)|("A"_kw,"C"_kw));
      terminal("TERM")(("A"_kw , "B"_kw)|("A"_kw,"C"_kw));
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  B").ret);
  EXPECT_TRUE(p.parse("RULE", " A C  ").ret);
  EXPECT_FALSE(p.parse("RULE", "A ").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "AB").ret);
  EXPECT_TRUE(p.parse("TERM", "AC").ret);
  EXPECT_FALSE(p.parse("TERM", " AB").ret);
  EXPECT_FALSE(p.parse("TERM", "AC ").ret);
}