#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>
#include <memory>
#include <ostream>
#include <variant>

using namespace pegium::parser;

namespace {

struct DummyElement final : pegium::grammar::AbstractElement {
  explicit DummyElement(ElementKind kind) : kind(kind) {}

  constexpr ElementKind getKind() const noexcept override { return kind; }
  void print(std::ostream &os) const override { os << "dummy"; }

  ElementKind kind;
};

} // namespace

TEST(DataTypeRuleTest, ParseRequiresFullConsumption) {
  DataTypeRule<std::string> rule{"Rule", ":"_kw};

  {
    auto result = rule.parse(":", ContextBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.len, 1u);
    EXPECT_EQ(result.value, ":");
  }

  {
    auto result = rule.parse(":abc", ContextBuilder().build());
    EXPECT_FALSE(result.ret);
    EXPECT_EQ(result.len, 1u);
  }
}

TEST(DataTypeRuleTest, StringRuleConcatenatesVisibleTokens) {
  TerminalRule<> ws{"WS", some(s)};
  DataTypeRule<std::string> rule{"Rule", "a"_kw + "b"_kw};

  auto result = rule.parse("a   b", ContextBuilder().ignore(ws).build());
  ASSERT_TRUE(result.ret);
  EXPECT_EQ(result.value, "ab");
}

TEST(DataTypeRuleTest, NonStringRuleRequiresValueConverter) {
  DataTypeRule<int> rule{"Rule", "123"_kw};
  EXPECT_THROW((void)rule.parse("123", ContextBuilder().build()),
               std::logic_error);
}

TEST(DataTypeRuleTest, ParseFailureRewindsCursor) {
  DataTypeRule<std::string> rule{"Rule", "abc"_kw};
  auto result = rule.parse("zzz", ContextBuilder().build());
  EXPECT_FALSE(result.ret);
  EXPECT_EQ(result.len, 0u);
}

TEST(DataTypeRuleTest, StringRuleHandlesCharacterRangeAndAnyCharacterKinds) {
  DataTypeRule<std::string> digit{"Digit", d};
  DataTypeRule<std::string> any{"Any", dot};

  {
    auto result = digit.parse("7", ContextBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.value, "7");
  }

  {
    auto result = any.parse("X", ContextBuilder().build());
    ASSERT_TRUE(result.ret);
    EXPECT_EQ(result.value, "X");
  }
}

TEST(DataTypeRuleTest, StringRuleUsesDefaultBranchForNonTerminalChildKind) {
  DataTypeRule<std::string> rule{"Rule", "ab"_kw};
  DummyElement group{pegium::grammar::ElementKind::Group};

  pegium::CstBuilder builder{"ab"};
  const char *begin = builder.input_begin();
  builder.enter(begin);
  builder.leaf(begin, begin + 2, &group, false);
  builder.exit(begin + 2, nullptr);
  auto root = builder.finalize();

  auto node = root->begin();
  ASSERT_NE(node, root->end());

  auto value = rule.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::string>(value));
  EXPECT_EQ(std::get<std::string>(value), "ab");
}

TEST(DataTypeRuleTest, GetValueAndParseGenericReturnRuleValueVariant) {
  DataTypeRule<std::string> rule{"Rule", "hello"_kw};

  auto parsed = rule.parse("hello", ContextBuilder().build());
  ASSERT_TRUE(parsed.ret);
  ASSERT_TRUE(parsed.root_node != nullptr);

  auto node =
      detail::findFirstMatchingNode(*parsed.root_node, std::addressof(rule));
  ASSERT_TRUE(node.has_value());

  auto value = rule.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::string>(value));
  EXPECT_EQ(std::get<std::string>(value), "hello");
  EXPECT_FALSE(rule.getTypeName().empty());

  auto generic = rule.parseGeneric("hello", ContextBuilder().build());
  ASSERT_TRUE(generic.root_node != nullptr);
}

TEST(DataTypeRuleTest, ParseRuleAddsNodeOnSuccessAndRewindsOnFailure) {
  DataTypeRule<std::string> rule{"Rule", "ab"_kw};
  auto context = ContextBuilder().build();

  {
    pegium::CstBuilder builder("ab");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ok = rule.parse_rule(state);
    EXPECT_TRUE(ok);
    EXPECT_EQ(state.cursor() - input.begin(), 2);

    auto root = builder.finalize();
    auto node = root->begin();
    ASSERT_NE(node, root->end());
    EXPECT_EQ((*node).getGrammarElement(), std::addressof(rule));
  }

  {
    pegium::CstBuilder builder("zz");
    const auto input = builder.getText();
    ParseState state{builder, context};

    auto ko = rule.parse_rule(state);
    EXPECT_FALSE(ko);
    EXPECT_EQ(state.cursor(), input.begin());

    auto root = builder.finalize();
    EXPECT_EQ(root->begin(), root->end());
  }
}
