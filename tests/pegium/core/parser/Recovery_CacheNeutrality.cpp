/// Cache-on / cache-off neutrality test suite.
///
/// The `ChoiceRecoverCache` is a pure optimization: disabling it must
/// never change the chosen recovery candidate. This suite asserts the
/// property at every build, on a small named set of recovery cases.
/// Each case exercises a distinct recovery modality with a stable
/// identifier so a regression points to the modality, not just to
/// "some test failed".
///
/// Cases:
///   1. delete_prefix_keyword          — leading noise before a keyword,
///                                       exercises delete-prefix retry
///                                       through OrderedChoice.
///   2. insert_missing_in_sequence     — missing keyword in a sequence
///                                       inside an OrderedChoice
///                                       alternative.
///   3. ordered_choice_branch_pick     — OrderedChoice where one branch
///                                       is the credible recovery and
///                                       the other branches are not.
///   4. repetition_with_inner_choice   — a Repetition whose body is an
///                                       OrderedChoice; cache hit
///                                       expected across iterations.
///   5. infix_operator_noise           — an Infix whose primary contains
///                                       an OrderedChoice; recovery
///                                       around operator noise.

#include "RecoveryTestSupport.hpp"
#include <pegium/core/RecoveryCacheNeutralityHarness.hpp>

using namespace pegium::parser;
using namespace pegium::test::recovery;
using pegium::test::expect_cache_neutral;

TEST(RecoveryCacheNeutrality, delete_prefix_keyword) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const auto skipper = SkipperBuilder().build();
  const std::string input = "xxxxservice";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseDataType(rule, input, skipper, opts);
      },
      "delete_prefix_keyword", input);
}

TEST(RecoveryCacheNeutrality, insert_missing_in_sequence) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNode> rule{
      "Rule", "hello"_kw + assign<&RecoveryNode::token>(id)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const std::string input = "world";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseRule(rule, input, skipper, opts);
      },
      "insert_missing_in_sequence", input);
}

TEST(RecoveryCacheNeutrality, ordered_choice_branch_pick) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNode> rule{
      "Rule", ("alpha"_kw + assign<&RecoveryNode::token>(id)) |
                  ("beta"_kw + assign<&RecoveryNode::token>(id)) |
                  ("gamma"_kw + assign<&RecoveryNode::token>(id))};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const std::string input = "delta name";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseRule(rule, input, skipper, opts);
      },
      "ordered_choice_branch_pick", input);
}

TEST(RecoveryCacheNeutrality, repetition_with_inner_choice) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryNode> item{
      "Item", ("a"_kw + assign<&RecoveryNode::token>(id)) |
                  ("b"_kw + assign<&RecoveryNode::token>(id))};
  ParserRule<RecoveryTransitionBlockNode> rule{
      "Rule", many(item) + "end"_kw};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const std::string input = "a foo b bar zzz nope end";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseRule(rule, input, skipper, opts);
      },
      "repetition_with_inner_choice", input);
}

TEST(RecoveryCacheNeutrality, infix_operator_noise) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<int> number{"NUMBER", some(d)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  ParserRule<RecoveryExpression> primary{
      "Primary", (create<RecoveryNumberExpression>() +
                  assign<&RecoveryNumberExpression::value>(number)) |
                     (create<RecoveryReferenceExpression>() +
                      assign<&RecoveryReferenceExpression::name>(id))};
  InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
            &RecoveryBinaryExpression::op, &RecoveryBinaryExpression::right>
      binary{"Binary", primary, LeftAssociation("+"_kw | "-"_kw)};
  ParserRule<RecoveryExpression> expression{"Expression", binary};
  // Wrap the expression in a ParserRule so we can use parseRule().
  ParserRule<RecoveryExpressionEvaluation> root{
      "Root", assign<&RecoveryExpressionEvaluation::expression>(expression)};
  const auto skipper = SkipperBuilder().ignore(ws).build();
  const std::string input = "1 +?? 2";
  expect_cache_neutral(
      [&](const ParseOptions &opts) {
        return parseRule(root, input, skipper, opts);
      },
      "infix_operator_noise", input);
}
