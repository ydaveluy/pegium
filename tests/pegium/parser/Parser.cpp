#include <gtest/gtest.h>
#include <pegium/TestCstBuilderHarness.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <sstream>

using namespace pegium::parser;

namespace {

struct FailingEntryNode : pegium::AstNode {};
struct KeywordChoiceNode : pegium::AstNode {};
struct TraceExpression;
struct TraceDefinition : pegium::AstNode {
  string name;
  pointer<TraceExpression> expr;
};
struct TraceExpression : pegium::AstNode {};
struct TraceNumberExpression : TraceExpression {
  int value = 0;
};
struct TraceBinaryExpression : TraceExpression {
  pointer<TraceExpression> left;
  string op;
  pointer<TraceExpression> right;
};
struct TraceEvaluation : pegium::AstNode {
  pointer<TraceExpression> expression;
};
struct TraceModule : pegium::AstNode {
  string name;
  vector<pointer<pegium::AstNode>> statements;
};
struct TraceUse : pegium::AstNode {
  reference<FailingEntryNode> target;
};
struct TraceBooleanChoice : pegium::AstNode {
  bool enabled = false;
};

bool has_keyword(const ExpectResult &expect, std::string_view keyword) {
  return std::ranges::any_of(expect.frontier, [&](const auto &path) {
    const auto *literal = path.literal();
    return literal != nullptr && literal->getValue() == keyword;
  });
}

bool has_rule(const ExpectResult &expect, std::string_view ruleName) {
  return std::ranges::any_of(expect.frontier, [&](const auto &path) {
    const auto *rule = path.expectedRule();
    return rule != nullptr && rule->getName() == ruleName;
  });
}

std::vector<std::string> describe_path(const ExpectPath &path) {
  std::vector<std::string> result;
  result.reserve(path.elements.size());
  for (const auto *element : path.elements) {
    if (element == nullptr) {
      result.push_back("<null>");
      continue;
    }
    switch (element->getKind()) {
    case pegium::grammar::ElementKind::Literal:
      result.push_back("keyword:" +
                       std::string(static_cast<const pegium::grammar::Literal *>(
                                       element)
                                       ->getValue()));
      break;
    case pegium::grammar::ElementKind::Assignment:
      result.push_back("assignment:" +
                       std::string(static_cast<const pegium::grammar::Assignment *>(
                                       element)
                                       ->getFeature()));
      break;
    case pegium::grammar::ElementKind::DataTypeRule:
    case pegium::grammar::ElementKind::ParserRule:
    case pegium::grammar::ElementKind::TerminalRule:
    case pegium::grammar::ElementKind::InfixRule:
      result.push_back("rule:" +
                       std::string(static_cast<const pegium::grammar::AbstractRule *>(
                                       element)
                                       ->getName()));
      break;
    default:
      result.push_back("other");
      break;
    }
  }
  return result;
}

class FailingEntryParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

  Rule<FailingEntryNode> Entry{"Entry", "service"_kw};
};

class KeywordChoiceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Model;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Rule<KeywordChoiceNode> Model{
      "Model", create<KeywordChoiceNode>() + ("entity"_kw | "enum"_kw)};
#pragma clang diagnostic pop
};

class DefinitionTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Definition;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<int> NUMBER{"NUMBER", some(d)};
  Rule<TraceExpression> Expression{
      "Expression", create<TraceNumberExpression>() +
                        assign<&TraceNumberExpression::value>(NUMBER)};
  Rule<TraceDefinition> Definition{
      "Definition",
      "def"_kw + assign<&TraceDefinition::name>(ID) + ":"_kw +
          assign<&TraceDefinition::expr>(Expression) + ";"_kw};
#pragma clang diagnostic pop
};

class ExpressionTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Evaluation;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<int> NUMBER{"NUMBER", some(d)};
  Rule<TraceExpression> Primary{
      "Primary", create<TraceNumberExpression>() +
                     assign<&TraceNumberExpression::value>(NUMBER)};
  InfixRule<TraceBinaryExpression, &TraceBinaryExpression::left,
            &TraceBinaryExpression::op, &TraceBinaryExpression::right>
      Expression{"Expression", Primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  Rule<TraceExpression> ExpressionRule{"Expression", Expression};
  Rule<TraceEvaluation> Evaluation{
      "Evaluation", assign<&TraceEvaluation::expression>(ExpressionRule) + ";"_kw};
#pragma clang diagnostic pop
};

class InfixDefinitionTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Definition;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<int> NUMBER{"NUMBER", some(d)};
  Rule<TraceExpression> Primary{
      "Primary",
      "("_kw + ")"_kw |
          create<TraceNumberExpression>() +
              assign<&TraceNumberExpression::value>(NUMBER)};
  InfixRule<TraceBinaryExpression, &TraceBinaryExpression::left,
            &TraceBinaryExpression::op, &TraceBinaryExpression::right>
      Expression{"Expression", Primary, LeftAssociation("*"_kw | "/"_kw),
                 LeftAssociation("+"_kw | "-"_kw)};
  Rule<TraceExpression> ExpressionRule{"Expression", Expression};
  Rule<TraceDefinition> Definition{
      "Definition",
      "def"_kw + assign<&TraceDefinition::name>(ID) + ":"_kw +
          assign<&TraceDefinition::expr>(ExpressionRule) + ";"_kw};
#pragma clang diagnostic pop
};

class ReferenceTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Use;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<TraceUse> Use{"Use", "use"_kw + assign<&TraceUse::target>(ID)};
#pragma clang diagnostic pop
};

class AssignmentOperatorTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<TraceBooleanChoice> Root{
      "Root", assign<&TraceBooleanChoice::enabled>("on"_kw) |
                  enable_if<&TraceBooleanChoice::enabled>("on"_kw)};
#pragma clang diagnostic pop
};

class ModuleTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Module;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Terminal<int> NUMBER{"NUMBER", some(d)};
  Rule<TraceExpression> Expression{
      "Expression", create<TraceNumberExpression>() +
                        assign<&TraceNumberExpression::value>(NUMBER)};
  Rule<TraceDefinition> Definition{
      "Definition",
      "def"_kw + assign<&TraceDefinition::name>(ID) + ":"_kw +
          assign<&TraceDefinition::expr>(Expression) + ";"_kw};
  Rule<TraceEvaluation> Evaluation{
      "Evaluation", assign<&TraceEvaluation::expression>(Expression) + ";"_kw};
  Rule<pegium::AstNode> Statement{"Statement", Definition | Evaluation};
  Rule<TraceModule> Module{
      "Module",
      "module"_kw + assign<&TraceModule::name>(ID) +
          many(append<&TraceModule::statements>(Statement))};
#pragma clang diagnostic pop
};

class OrderedChoiceSuffixTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          "start"_kw + ("left"_kw | "right"_kw) + ";"_kw};
#pragma clang diagnostic pop
};

class OptionalRepetitionTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry", create<KeywordChoiceNode>() + many("entry"_kw) + "end"_kw};
#pragma clang diagnostic pop
};

class RequiredRepetitionTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Rule<KeywordChoiceNode> Entry{
      "Entry", create<KeywordChoiceNode>() + some("entry"_kw) + "end"_kw};
#pragma clang diagnostic pop
};

class ZeroWidthTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() + &("def"_kw) + !("x"_kw) + "def"_kw};
#pragma clang diagnostic pop
};

class NullablePrefixTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          option("public"_kw) + ("def"_kw | "enum"_kw)};
#pragma clang diagnostic pop
};

class OptionalOrderedChoiceTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          "start"_kw + ("left"_kw | option("right"_kw)) + "end"_kw};
#pragma clang diagnostic pop
};

class PositivePredicateTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          "@"_kw + ((&"a"_kw) + "a"_kw | (&"b"_kw) + "b"_kw)};
#pragma clang diagnostic pop
};

class NegativePredicateTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          "!"_kw + ((!("="_kw)) + ID | "="_kw)};
#pragma clang diagnostic pop
};

class UnorderedSuffixTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() + ("a"_kw & "b"_kw) + "!"_kw};
#pragma clang diagnostic pop
};

class LocalSkipperTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Terminal<> HiddenWs{"WS", some(s)};
  Skipper skipper = skip();
  Rule<KeywordChoiceNode> Entry{
      "Entry",
      create<KeywordChoiceNode>() +
          ("("_kw + "value"_kw + ")"_kw).skip(ignored(HiddenWs)) + ";"_kw};
#pragma clang diagnostic pop
};

class DataTypeFrontTraceParser final : public PegiumParser {
protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Entry;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  DataTypeRule<std::string> Value{"VALUE", "true"_kw | "false"_kw};
  Rule<KeywordChoiceNode> Entry{
      "Entry", create<KeywordChoiceNode>() + Value + ";"_kw};
#pragma clang diagnostic pop
};

} // namespace

TEST(ParserTest, NoOpSkipperSkipsNothing) {
  auto context = NoOpSkipper();

  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto result = context.skip(input.begin(), builder);
  EXPECT_EQ(result, input.begin());
}

TEST(ParserTest, EndOfFileExpressionWorks) {
  auto expr = "a"_kw + eof;

  {
    std::string okInput = "a";
    auto ok = expr.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 1);
  }

  {
    std::string koInput = "ab";
    auto ko = expr.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, NegatedCharacterRangeLiteralWorks) {
  auto notNewLine = "^\n"_cr;

  {
    std::string okInput = "x";
    auto ok = notNewLine.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 1);
  }

  {
    std::string koInput = "\n";
    auto ko = notNewLine.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, UntilOperatorConsumesUntilClosingToken) {
  auto comment = "/*"_kw <=> "*/"_kw;

  {
    std::string okInput = "/*hello*/";
    auto ok = comment.terminal(okInput);
    EXPECT_NE(ok, nullptr);
    EXPECT_EQ(ok - (okInput).c_str(), 9);
  }

  {
    std::string koInput = "/*hello";
    auto ko = comment.terminal(koInput);
    EXPECT_EQ(ko, nullptr);
  }
}

TEST(ParserTest, EntryRuleFailureLeavesTopLevelCstEmpty) {
  FailingEntryParser parser;
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText("");
  parser.parse(*document);

  ASSERT_NE(document->parseResult.cst, nullptr);
  EXPECT_EQ(document->parseResult.cst->begin(), document->parseResult.cst->end());
  EXPECT_FALSE(document->parseSucceeded());
}

TEST(ParserTest, ExpectReturnsKeywordAlternativesAtRoot) {
  KeywordChoiceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_EQ(expect.frontier.size(), 2u);
  EXPECT_TRUE(expect.frontier.front().expectsKeyword());
  ASSERT_NE(expect.frontier.front().literal(), nullptr);
  EXPECT_EQ(expect.frontier.front().literal()->getValue(), "entity");
  ASSERT_NE(expect.frontier.back().literal(), nullptr);
  EXPECT_EQ(expect.frontier.back().literal()->getValue(), "enum");
}

TEST(ParserTest, ExpectReturnsDefinitionFrontierAfterKeyword) {
  DefinitionTraceParser parser;
  const auto expect = parser.expect("def ", 4);

  ASSERT_EQ(expect.frontier.size(), 1u);
  EXPECT_TRUE(expect.frontier.front().expectsRule());
  ASSERT_NE(expect.frontier.front().expectedRule(), nullptr);
  EXPECT_EQ(expect.frontier.front().expectedRule()->getName(), "ID");
  EXPECT_EQ(describe_path(expect.frontier.front()),
            (std::vector<std::string>{
                "rule:Definition", "assignment:name", "rule:ID"}));
}

TEST(ParserTest, ExpectReturnsExpressionFrontierAfterInfixOperator) {
  ExpressionTraceParser parser;
  const auto expect = parser.expect("2 *", 3);

  ASSERT_EQ(expect.frontier.size(), 1u);
  ASSERT_NE(expect.frontier.front().expectedRule(), nullptr);
  EXPECT_EQ(expect.frontier.front().expectedRule()->getName(), "NUMBER");
}

TEST(ParserTest, ExpectExpandsExpressionEntryFrontier) {
  InfixDefinitionTraceParser parser;
  const auto expect = parser.expect("def a : ", 8);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(std::ranges::any_of(
      expect.frontier, [](const auto &item) {
        const auto *literal = item.literal();
        return literal != nullptr && literal->getValue() == "(";
      }));
}

TEST(ParserTest, ExpectReturnsReferenceFrontier) {
  ReferenceTraceParser parser;
  const auto expect = parser.expect("use ", 4);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(expect.frontier.front().expectsReference());
  EXPECT_EQ(describe_path(expect.frontier.front()),
            (std::vector<std::string>{"rule:Use", "assignment:target"}));
}

TEST(ParserTest, ExpectKeepsDistinctAssignmentOperatorsInFrontier) {
  AssignmentOperatorTraceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_EQ(expect.frontier.size(), 2u);
  EXPECT_TRUE(std::ranges::all_of(expect.frontier, [](const auto &path) {
    const auto *literal = path.literal();
    return literal != nullptr && literal->getValue() == "on";
  }));
  EXPECT_TRUE(std::ranges::any_of(expect.frontier, [](const auto &path) {
    const auto *assignment = path.contextAssignment();
    return assignment != nullptr &&
           assignment->getOperator() ==
               pegium::grammar::AssignmentOperator::Assign;
  }));
  EXPECT_TRUE(std::ranges::any_of(expect.frontier, [](const auto &path) {
    const auto *assignment = path.contextAssignment();
    return assignment != nullptr &&
           assignment->getOperator() ==
               pegium::grammar::AssignmentOperator::EnableIf;
  }));
}

TEST(ParserTest,
     ExpectIncludesKeywordAlternativeAfterOptionalRepetition) {
  ModuleTraceParser parser;
  constexpr std::string_view text = "module name\n";
  const auto expect = parser.expect(text, text.size());

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(std::ranges::any_of(
      expect.frontier, [](const auto &item) {
        const auto *literal = item.literal();
        return literal != nullptr && literal->getValue() == "def";
      }));
}

TEST(ParserTest,
     ExpectIncludesKeywordAlternativeInsideSkippedTriviaBeforeStatement) {
  ModuleTraceParser parser;
  constexpr std::string_view text =
      "module name\n"
      "\n"
      "def value : 0 ;";
  const auto expect = parser.expect(text, 12);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(std::ranges::any_of(
      expect.frontier, [](const auto &item) {
        const auto *literal = item.literal();
        return literal != nullptr && literal->getValue() == "def";
      }));
}

TEST(ParserTest, ExpectKeepsOnlyDirectOrderedChoiceFrontier) {
  OrderedChoiceSuffixTraceParser parser;
  const auto expect = parser.expect("start", 5);

  ASSERT_EQ(expect.frontier.size(), 2u);
  EXPECT_TRUE(has_keyword(expect, "left"));
  EXPECT_TRUE(has_keyword(expect, "right"));
  EXPECT_FALSE(has_keyword(expect, ";"));
}

TEST(ParserTest,
     ExpectIncludesExitAndContinuationAtOptionalRepetitionRoot) {
  OptionalRepetitionTraceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "entry"));
  EXPECT_TRUE(has_keyword(expect, "end"));
}

TEST(ParserTest,
     ExpectIncludesExitAndContinuationAfterRequiredRepetition) {
  RequiredRepetitionTraceParser parser;
  const auto expect = parser.expect("entry ", 6);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "entry"));
  EXPECT_TRUE(has_keyword(expect, "end"));
}

TEST(ParserTest,
     ExpectIncludesParentExitAfterCompleteInfixOperand) {
  ExpressionTraceParser parser;
  const auto expect = parser.expect("2", 1);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, ";"));
  EXPECT_TRUE(has_keyword(expect, "+"));
  EXPECT_TRUE(has_keyword(expect, "*"));
}

TEST(ParserTest, ExpectDoesNotExposeZeroWidthElements) {
  ZeroWidthTraceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_EQ(expect.frontier.size(), 1u);
  ASSERT_NE(expect.frontier.front().literal(), nullptr);
  EXPECT_EQ(expect.frontier.front().literal()->getValue(), "def");
}

TEST(ParserTest,
     ExpectIncludesNullablePrefixAndFollowingChoice) {
  NullablePrefixTraceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "public"));
  EXPECT_TRUE(has_keyword(expect, "def"));
  EXPECT_TRUE(has_keyword(expect, "enum"));
}

TEST(ParserTest,
     ExpectIncludesExitAlongsideOrderedChoiceBranch) {
  OptionalOrderedChoiceTraceParser parser;
  const auto expect = parser.expect("start", 5);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "left"));
  EXPECT_TRUE(has_keyword(expect, "right"));
  EXPECT_TRUE(has_keyword(expect, "end"));
}

TEST(ParserTest, ExpectFiltersPositivePredicateBranchesAtAnchor) {
  PositivePredicateTraceParser parser;
  const auto expect = parser.expect("@a", 1);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "a"));
  EXPECT_FALSE(has_keyword(expect, "b"));
}

TEST(ParserTest, ExpectFiltersNegativePredicateBranchesAtAnchor) {
  NegativePredicateTraceParser parser;
  const auto expect = parser.expect("!=", 1);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "="));
  EXPECT_FALSE(has_rule(expect, "ID"));
}

TEST(ParserTest, ExpectReturnsRemainingElementInsideUnorderedGroup) {
  UnorderedSuffixTraceParser parser;
  const auto expect = parser.expect("a", 1);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "b"));
}

TEST(ParserTest,
     ExpectHonorsLocalSkipperInsideExpressionBeforeAnchor) {
  LocalSkipperTraceParser parser;
  const auto expect = parser.expect("(   ", 4);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, "value"));
}

TEST(ParserTest, ExpectKeepsDatatypeRuleOpaqueAtFrontier) {
  DataTypeFrontTraceParser parser;
  const auto expect = parser.expect("", 0);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_rule(expect, "VALUE"));
  EXPECT_FALSE(has_keyword(expect, "true"));
  EXPECT_FALSE(has_keyword(expect, "false"));
}

TEST(ParserTest,
     ExpectIncludesParentExitAfterCompleteInfixRightOperand) {
  ExpressionTraceParser parser;
  const auto expect = parser.expect("2 + 3", 5);

  ASSERT_FALSE(expect.frontier.empty());
  EXPECT_TRUE(has_keyword(expect, ";"));
  EXPECT_TRUE(has_keyword(expect, "+"));
  EXPECT_TRUE(has_keyword(expect, "*"));
}

TEST(ParserTest, ParseDiagnosticKindStreamUsesCanonicalName) {
  std::ostringstream stream;
  stream << ParseDiagnosticKind::ConversionError;
  EXPECT_EQ(stream.str(), "ConversionError");
}

TEST(ParserTest, ParseDiagnosticStreamSerializesFields) {
  ParseDiagnostic diagnostic{
      .kind = ParseDiagnosticKind::Incomplete,
      .offset = 12,
      .beginOffset = 10,
      .endOffset = 14,
      .element = nullptr,
      .message = "missing token",
  };

  std::ostringstream stream;
  stream << diagnostic;

  EXPECT_EQ(stream.str(),
            "ParseDiagnostic{kind=Incomplete, offset=12, begin=10, end=14, "
            "message=\"missing token\"}");
}
