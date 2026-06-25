#include <gtest/gtest.h>
#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

#include <string>
#include <utility>

using namespace pegium::parser;

namespace {
struct Expr : pegium::AstNode {};
struct Num : Expr {
  string value;
};
struct Bin : Expr {
  pointer<Expr> left;
  string op;
  pointer<Expr> right;
};

// A C-like operator grammar with prefix-overlapping operators (`|`/`||`,
// `&`/`&&`, `<`/`<<`/`<=`, `>`/`>>`/`>=`) declared with PLAIN literals. The
// infix maximal-munch guard makes a shorter operator yield to a longer declared
// operator that also matches — no hand-written `!"|"` lookahead needed.
struct CLikeParser final : PegiumParser {
  Terminal<> WS{"WS", some(s)};
  Terminal<std::string> NUM{"NUM", some(d)};
  Rule<Expr> Primary{"Primary", create<Num>() + assign<&Num::value>(NUM)};
  Infix<Bin, &Bin::left, &Bin::op, &Bin::right> Binary{
      "Binary", Primary,
      LeftAssociation("*"_kw | "/"_kw | "%"_kw),
      LeftAssociation("+"_kw | "-"_kw),
      LeftAssociation("<<"_kw | ">>"_kw),
      LeftAssociation(">="_kw | "<="_kw | ">"_kw | "<"_kw),
      LeftAssociation("=="_kw | "!="_kw),
      LeftAssociation("&"_kw),
      LeftAssociation("^"_kw),
      LeftAssociation("|"_kw),
      LeftAssociation("&&"_kw),
      LeftAssociation("||"_kw)};
  Rule<Expr> Root{"Root", Binary};
  Skipper skipper = SkipperBuilder().ignore(WS).build();
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }
  const Skipper &getSkipper() const noexcept override { return skipper; }
};

std::string opOf(const pegium::AstNode *value) {
  const auto *bin = pegium::ast_ptr_cast<const Bin>(value);
  return bin != nullptr ? bin->op : std::string{"<not-a-Bin>"};
}
} // namespace

TEST(InfixMaximalMunch, ShorterOperatorYieldsToLonger) {
  CLikeParser p;
  for (const auto [in, want] : {
           std::pair{"1||2", "||"}, {"1&&2", "&&"}, {"1|2", "|"},
           {"1&2", "&"},   {"1<<2", "<<"}, {"1>>2", ">>"},
           {"1<2", "<"},   {"1>2", ">"},   {"1<=2", "<="},
           {"1>=2", ">="}, {"1==2", "=="}, {"1!=2", "!="},
       }) {
    auto r = pegium::test::Parse(p, in);
    EXPECT_TRUE(r.fullMatch) << in;
    EXPECT_EQ(opOf(r.value), std::string(want)) << in;
  }
}

TEST(InfixMaximalMunch, PrecedenceIsPreserved) {
  CLikeParser p;
  // `||` binds looser than `&&`: `1 || 2 && 3` parses as `1 || (2 && 3)`.
  auto r = pegium::test::Parse(p, "1 || 2 && 3");
  ASSERT_TRUE(r.fullMatch);
  const auto *top = pegium::ast_ptr_cast<const Bin>(r.value);
  ASSERT_NE(top, nullptr);
  EXPECT_EQ(top->op, "||");
  const auto *right = pegium::ast_ptr_cast<const Bin>(top->right);
  ASSERT_NE(right, nullptr);
  EXPECT_EQ(right->op, "&&");
}
