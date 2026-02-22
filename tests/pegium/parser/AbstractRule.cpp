#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>

using namespace pegium::parser;

namespace {

struct DummyRule final : AbstractRule {
  using AbstractRule::AbstractRule;
  using AbstractRule::operator=;

  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::DataTypeRule;
  }
  pegium::grammar::RuleValue
  getValue(const pegium::CstNodeView &) const override {
    return std::string_view{};
  }
  std::string_view getTypeName() const noexcept override { return {}; }
};

} // namespace

TEST(AbstractRuleTest, ParseTerminalUsesAssignedExpression) {
  DummyRule rule{"Rule", "a"_kw};
  std::string_view input = "abc";

  auto result = rule.parse_terminal(input);
  EXPECT_TRUE(result.IsValid());
  EXPECT_EQ(result.offset - input.begin(), 1);
}

TEST(AbstractRuleTest, ReassignmentUpdatesSuperImplementation) {
  DummyRule rule{"Rule", "a"_kw};
  rule = "b"_kw;

  std::string_view okInput = "bcd";
  auto ok = rule.super().parse_terminal(okInput);
  EXPECT_TRUE(ok.IsValid());
  EXPECT_EQ(ok.offset - okInput.begin(), 1);

  std::string_view koInput = "acd";
  auto ko = rule.super().parse_terminal(koInput);
  EXPECT_FALSE(ko.IsValid());
}

TEST(AbstractRuleTest, GetElementTracksAssignedExpression) {
  DummyRule rule{"Rule", "a"_kw};
  const auto *first = rule.getElement();
  EXPECT_EQ(first, rule.super().getElement());

  rule = "b"_kw;
  const auto *second = rule.getElement();
  EXPECT_EQ(second, rule.super().getElement());
  EXPECT_NE(second, first);
}

TEST(AbstractRuleTest, SuperParseRuleUsesCurrentAssignedExpression) {
  auto context = ContextBuilder().build();
  DummyRule rule{"Rule", "a"_kw};

  {
    pegium::CstBuilder builder("b");
    const auto input = builder.getText();
    ParseState state{builder, context};
    EXPECT_FALSE(rule.super().parse_rule(state));
    EXPECT_EQ(state.cursor(), input.begin());
  }

  rule = "b"_kw;
  {
    pegium::CstBuilder builder("b");
    const auto input = builder.getText();
    ParseState state{builder, context};
    EXPECT_TRUE(rule.super().parse_rule(state));
    EXPECT_EQ(state.cursor() - input.begin(), 1);
  }
}

TEST(AbstractRuleTest, CanBeDestroyedThroughGrammarAbstractRulePointer) {
  std::unique_ptr<pegium::grammar::AbstractRule> rule =
      std::make_unique<DummyRule>("Rule", "a"_kw);
  ASSERT_TRUE(rule != nullptr);
}
