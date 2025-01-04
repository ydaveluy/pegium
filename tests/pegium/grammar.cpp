
#include <gtest/gtest.h>
#include <pegium/Parser.hpp>

struct Attribute {
  std::string type;
};

template <typename RuleType, typename AttrType>
concept IsValidRule = std::derived_from<std::decay_t<RuleType>, pegium::grammar::IRule> &&
                          requires(RuleType rule, const pegium::CstNode &node) {
                            {
                              rule.getValue(node)
                            } -> std::convertible_to<AttrType>;
                          } ||
                      std::same_as<bool, AttrType>;

template <typename T> struct IsOrderedChoice : std::false_type {};

template <typename... E>
struct IsOrderedChoice<pegium::grammar::OrderedChoice<E...>> : std::true_type {

  template <typename AttrType> static constexpr bool IsValidRule() {
    return (IsValidRule<E, AttrType> && ...);
  }
};

template <typename AttrType, typename RuleType>
concept CompatibleAttribute =
    IsValidRule<RuleType, AttrType> ||
    (IsOrderedChoice<RuleType>::value &&
     IsOrderedChoice<RuleType>::template IsValidRule<AttrType>());

template <typename ClassType, typename AttrType, typename RuleType>
struct Assignment2 : pegium::grammar::IElement {
public:
  Assignment2(AttrType ClassType::*feature, RuleType &&rule)
      : feature_(feature), rule_(std::forward<RuleType>(rule)) {}

  virtual std::size_t parse_terminal(std::string_view sv) const noexcept {
    return 0;
  };
  // parse the input text from a rule: hidden/ignored token between elements are
  // skipped
  virtual std::size_t parse_rule(std::string_view sv, pegium::CstNode &parent,
    pegium::grammar::IContext &c) const {
    return 0;
  };
  AttrType ClassType::*feature_;
  RuleType rule_;
};
template <typename ClassType, typename AttrType, typename RuleType>
  requires pegium::grammar::IsGrammarElement<RuleType> && CompatibleAttribute<AttrType, RuleType>
Assignment2<ClassType, AttrType, RuleType>
operator+=(AttrType ClassType::*feature, RuleType &&rule) {

  return Assignment2<ClassType, AttrType, RuleType>(
      feature, std::forward<RuleType>(rule));
}

//
//using namespace pegium::grammar;
TEST(GrammarTest, Literal) {
  class Parser : public pegium::Parser {
  public:
  pegium::grammar::TerminalRule<> WS{this, "WS"};
  pegium::grammar::DataTypeRule<> RULE{this, "RULE"};
  pegium::grammar::TerminalRule<> TERM{this, "TERM"};

    Parser() {
        using namespace pegium::grammar;
      WS = at_least_one(s);
      RULE = TERM | WS;
      TERM = "test"_kw + (&Attribute::type += WS);
    }

    std::unique_ptr<pegium::grammar::IContext> createContext() const override {
      return makeContext(std::tie(WS));
    }
  };
  Parser p;

  EXPECT_FALSE(p.RULE.parse(" ").ret);
  EXPECT_TRUE(p.RULE.parse("test").ret);
  EXPECT_TRUE(p.RULE.parse("  test  ").ret);
  /*EXPECT_TRUE(p.parse("RULE", "  test  ").ret);
  EXPECT_FALSE(p.parse("RULE", "test test").ret);
  EXPECT_FALSE(p.parse("RULE", "testtest").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", "  test  ").value)),
            "test");

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test ").ret);
  EXPECT_FALSE(p.parse("TERM", " test").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "test").value)),
            "test");*/
}
/*
TEST(GrammarTest, CharactersRanges) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = "a-e0-2j"_cr;
      terminal("TERM") = "a-e0-2j"_cr;
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", " ").ret);
  EXPECT_TRUE(p.parse("RULE", "  a  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  e  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  j  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  0  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  2  ").ret);
  EXPECT_FALSE(p.parse("RULE", " f ").ret);
  EXPECT_FALSE(p.parse("RULE", " 4 ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", "  a  ").value)), "a");

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "a").ret);
  EXPECT_TRUE(p.parse("TERM", "e").ret);
  EXPECT_TRUE(p.parse("TERM", "0").ret);
  EXPECT_TRUE(p.parse("TERM", "e").ret);
  EXPECT_TRUE(p.parse("TERM", "j").ret);
  EXPECT_FALSE(p.parse("TERM", "f").ret);
  EXPECT_FALSE(p.parse("TERM", "5").ret);
  EXPECT_FALSE(p.parse("TERM", "g").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "e").value)), "e");
}

TEST(GrammarTest, Optional) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = opt("test"_kw);
      terminal("TERM") = opt("test"_kw);
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", " ").ret);
  EXPECT_TRUE(p.parse("RULE", "  test  ").ret);
  EXPECT_FALSE(p.parse("RULE", "test test").ret);
  EXPECT_FALSE(p.parse("RULE", "testtest").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", "    ").value)), "");
  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", "  test  ").value)),
            "test");

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test ").ret);
  EXPECT_FALSE(p.parse("TERM", " test").ret);
  EXPECT_FALSE(p.parse("TERM", "testtest").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "").value)), "");
  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "test").value)),
            "test");
}

TEST(GrammarTest, Many) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = many("test"_kw);
      terminal("TERM") = many("test"_kw);
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test test test").ret);

  EXPECT_EQ(
      (std::any_cast<std::string>(p.parse("RULE", " test  test   ").value)),
      "testtest");

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttesttesttest").ret);
  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test ").ret);
  EXPECT_FALSE(p.parse("TERM", " test").ret);
  EXPECT_FALSE(p.parse("TERM", "testtest ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "testtest").value)),
            "testtest");
}

TEST(GrammarTest, ManySep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = many_sep("."_kw, "test"_kw);
      terminal("TERM") = many_sep("."_kw, "test"_kw);
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", ".").ret);
  EXPECT_FALSE(p.parse("RULE", "test.").ret);
  EXPECT_TRUE(p.parse("RULE", "").ret);

  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", " test . test ").ret);
  EXPECT_TRUE(p.parse("RULE", "test.test.test. test.test").ret);

  EXPECT_EQ(
      (std::any_cast<std::string>(p.parse("RULE", " test  . test   ").value)),
      "test.test");

  EXPECT_FALSE(p.parse("TERM", " ").ret);
  EXPECT_FALSE(p.parse("TERM", "test .").ret);
  EXPECT_FALSE(p.parse("TERM", " test.").ret);
  EXPECT_FALSE(p.parse("TERM", "test.test ").ret);

  EXPECT_TRUE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test.test.test.test").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "test.test").value)),
            "test.test");
}

TEST(GrammarTest, AtLeastOne) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = at_least_one("test"_kw);
      terminal("TERM") = at_least_one("test"_kw);
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_FALSE(p.parse("RULE", "testtest").ret);
  EXPECT_TRUE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test test test").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", " test  ").value)),
            "test");

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_FALSE(p.parse("TERM", "test test").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttesttesttest").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "test").value)),
            "test");
}
TEST(GrammarTest, AtLeastOneSep) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = at_least_one_sep("."_kw, "test"_kw);
      terminal("TERM") = at_least_one_sep("."_kw, "test"_kw);
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_FALSE(p.parse("RULE", ".").ret);
  EXPECT_FALSE(p.parse("RULE", "test.").ret);
  EXPECT_TRUE(p.parse("RULE", "test ").ret);
  EXPECT_TRUE(p.parse("RULE", "test .test").ret);
  EXPECT_TRUE(p.parse("RULE", "  test.test . test.test.test  ").ret);

  EXPECT_EQ(
      (std::any_cast<std::string>(p.parse("RULE", " test  . test   ").value)),
      "test.test");

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_FALSE(p.parse("TERM", ".").ret);
  EXPECT_FALSE(p.parse("TERM", "test.").ret);
  EXPECT_FALSE(p.parse("TERM", "test .test").ret);
  EXPECT_TRUE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test").ret);
  EXPECT_TRUE(p.parse("TERM", "test.test.test.test.test").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "test.test").value)),
            "test.test");
}

TEST(GrammarTest, Repetition) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = rep<2, 3>("test"_kw);
      terminal("TERM") = rep<2, 3>("test"_kw);
    }
  };
  Parser p;
  EXPECT_FALSE(p.parse("RULE", "test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test").ret);
  EXPECT_TRUE(p.parse("RULE", "test test test").ret);
  EXPECT_FALSE(p.parse("RULE", "test test test test").ret);

  EXPECT_EQ(
      (std::any_cast<std::string>(p.parse("RULE", " test   test   ").value)),
      "testtest");

  EXPECT_FALSE(p.parse("TERM", "test").ret);
  EXPECT_TRUE(p.parse("TERM", "testtest").ret);
  EXPECT_TRUE(p.parse("TERM", "testtesttest").ret);
  EXPECT_FALSE(p.parse("TERM", "testtesttesttest").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "testtest").value)),
            "testtest");
}

TEST(GrammarTest, Group) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = "A"_kw + "B"_kw;
      terminal("TERM") = "A"_kw + "B"_kw;
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  B").ret);
  EXPECT_FALSE(p.parse("RULE", "A ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", " A   B  ").value)),
            "AB");

  EXPECT_FALSE(p.parse("TERM", "A").ret);
  EXPECT_TRUE(p.parse("TERM", "AB").ret);
  EXPECT_FALSE(p.parse("TERM", " AB").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "AB").value)), "AB");
}

TEST(GrammarTest, UnorderedGroup) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = "A"_kw & "B"_kw & "C"_kw;
      terminal("TERM") = "A"_kw & "B"_kw & "C"_kw;
    }
  };
  Parser p;

  EXPECT_TRUE(p.parse("RULE", "  A  B C").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  C B").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  A C").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  C A").ret);
  EXPECT_TRUE(p.parse("RULE", "  C  A B").ret);
  EXPECT_TRUE(p.parse("RULE", "  C  B A").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", " A  C B  ").value)),
            "ACB");

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

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "ACB").value)), "ACB");
}

TEST(GrammarTest, PrioritizedChoice) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = "A"_kw | "B"_kw;
      terminal("TERM") = "A"_kw | "B"_kw;
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  ").ret);
  EXPECT_TRUE(p.parse("RULE", "  B  ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", " A     ").value)),
            "A");
  EXPECT_FALSE(p.parse("RULE", "A B").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "A").ret);
  EXPECT_TRUE(p.parse("TERM", "B").ret);
  EXPECT_FALSE(p.parse("TERM", " A").ret);
  EXPECT_FALSE(p.parse("TERM", "A ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "A").value)), "A");
}

TEST(GrammarTest, PrioritizedChoiceWithGroup) {
  class Parser : public pegium::Parser {
  public:
    Parser() {
      using namespace pegium::grammar;
      terminal("WS").ignore() = +s;
      rule("RULE") = "A"_kw + "B"_kw | "A"_kw + "C"_kw;
      terminal("TERM") = "A"_kw + "B"_kw | "A"_kw + "C"_kw;
    }
  };
  Parser p;

  EXPECT_FALSE(p.parse("RULE", "").ret);
  EXPECT_TRUE(p.parse("RULE", "  A  B").ret);
  EXPECT_TRUE(p.parse("RULE", " A C  ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("RULE", " A   B  ").value)),
            "AB");

  EXPECT_FALSE(p.parse("RULE", "A ").ret);

  EXPECT_FALSE(p.parse("TERM", "").ret);
  EXPECT_TRUE(p.parse("TERM", "AB").ret);
  EXPECT_TRUE(p.parse("TERM", "AC").ret);
  EXPECT_FALSE(p.parse("TERM", " AB").ret);
  EXPECT_FALSE(p.parse("TERM", "AC ").ret);

  EXPECT_EQ((std::any_cast<std::string>(p.parse("TERM", "AB").value)), "AB");
}*/