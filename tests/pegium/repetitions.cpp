
#include <gtest/gtest.h>

#include <pegium/Parser.hpp>

TEST(RepetitionTest, Optional) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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

TEST(RepetitionTest, Many) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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

TEST(RepetitionTest, ManySep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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

TEST(RepetitionTest, AtLeastOne) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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
TEST(RepetitionTest, AtLeastOneSep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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

TEST(RepetitionTest, Repetition) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium;
      terminal("WS").ignore()(+s());
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
