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
///   6. nested_last_element_choice_under_follow
///                                     — an OrderedChoice that is the LAST
///                                       element of an inner Group which is
///                                       itself followed by a keyword, so the
///                                       choice recovers under the follow
///                                       inherited from the outer scope (the
///                                       pass-through follow-probe path). Pins
///                                       cache-neutrality of the inherited-
///                                       follow fingerprint.

#include "RecoveryTestSupport.hpp"
#include <pegium/core/RecoveryCacheNeutralityHarness.hpp>

#include <functional>
#include <string>
#include <string_view>

using namespace pegium::parser;
using namespace pegium::test::recovery;
using pegium::test::expect_cache_neutral;

// Table-driven fold of the per-modality cache-neutrality cases.
//
// Each row owns its grammar construction VERBATIM from the original
// standalone TEST: because the grammar objects hold internal references
// to one another (e.g. a rule references its sub-rules, an Infix
// references its primary) and must stay at fixed addresses for the whole
// duration of both parses, they live as locals inside the row's `build`
// closure, whose stack frame stays active across the single
// `expect_cache_neutral(...)` call. The harness — and the entire
// assertion sequence — is therefore unchanged; only the surrounding
// scaffolding is shared.
namespace {

struct Case {
  const char *name;
  // Constructs the grammar and runs the unchanged neutrality harness.
  std::function<void(const char *name, std::string_view input)> build;
  std::string_view input;
};

const Case kCases[] = {
    {"delete_prefix_keyword",
     [](const char *name, std::string_view input) {
       DataTypeRule<std::string> rule{"Rule", "service"_kw};
       const auto skipper = SkipperBuilder().build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseDataType(rule, input, skipper, opts);
           },
           name, input);
     },
     "xxxxservice"},
    {"insert_missing_in_sequence",
     [](const char *name, std::string_view input) {
       TerminalRule<> ws{"WS", some(s)};
       TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
       ParserRule<RecoveryNode> rule{
           "Rule", "hello"_kw + assign<&RecoveryNode::token>(id)};
       const auto skipper = SkipperBuilder().ignore(ws).build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseRule(rule, input, skipper, opts);
           },
           name, input);
     },
     "world"},
    {"ordered_choice_branch_pick",
     [](const char *name, std::string_view input) {
       TerminalRule<> ws{"WS", some(s)};
       TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
       ParserRule<RecoveryNode> rule{
           "Rule", ("alpha"_kw + assign<&RecoveryNode::token>(id)) |
                       ("beta"_kw + assign<&RecoveryNode::token>(id)) |
                       ("gamma"_kw + assign<&RecoveryNode::token>(id))};
       const auto skipper = SkipperBuilder().ignore(ws).build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseRule(rule, input, skipper, opts);
           },
           name, input);
     },
     "delta name"},
    {"repetition_with_inner_choice",
     [](const char *name, std::string_view input) {
       TerminalRule<> ws{"WS", some(s)};
       TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
       ParserRule<RecoveryNode> item{
           "Item", ("a"_kw + assign<&RecoveryNode::token>(id)) |
                       ("b"_kw + assign<&RecoveryNode::token>(id))};
       ParserRule<RecoveryTransitionBlockNode> rule{
           "Rule", many(item) + "end"_kw};
       const auto skipper = SkipperBuilder().ignore(ws).build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseRule(rule, input, skipper, opts);
           },
           name, input);
     },
     "a foo b bar zzz nope end"},
    {"infix_operator_noise",
     [](const char *name, std::string_view input) {
       TerminalRule<> ws{"WS", some(s)};
       TerminalRule<int> number{"NUMBER", some(d)};
       TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
       ParserRule<RecoveryExpression> primary{
           "Primary", (create<RecoveryNumberExpression>() +
                       assign<&RecoveryNumberExpression::value>(number)) |
                          (create<RecoveryReferenceExpression>() +
                           assign<&RecoveryReferenceExpression::name>(id))};
       InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
                 &RecoveryBinaryExpression::op,
                 &RecoveryBinaryExpression::right>
           binary{"Binary", primary, LeftAssociation("+"_kw | "-"_kw)};
       ParserRule<RecoveryExpression> expression{"Expression", binary};
       // Wrap the expression in a ParserRule so we can use parseRule().
       ParserRule<RecoveryExpressionEvaluation> root{
           "Root",
           assign<&RecoveryExpressionEvaluation::expression>(expression)};
       const auto skipper = SkipperBuilder().ignore(ws).build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseRule(root, input, skipper, opts);
           },
           name, input);
     },
     "1 +?? 2"},
    {"nested_last_element_choice_under_follow",
     [](const char *name, std::string_view input) {
       TerminalRule<> ws{"WS", some(s)};
       TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
       // The OrderedChoice is the LAST element of the inner Group
       // ("open"_kw + choice); that inner Group is in turn followed by
       // "close"_kw. So while the choice recovers it inherits the OUTER
       // follow ("close") via the pass-through follow-probe path, and the
       // inherited-follow identity is folded into the choice-recover cache
       // fingerprint. This case pins that the inherited-follow fingerprint
       // is cache-neutral.
       ParserRule<RecoveryNode> rule{
           "Rule",
           ("open"_kw + (("alpha"_kw + assign<&RecoveryNode::token>(id)) |
                         ("beta"_kw + assign<&RecoveryNode::token>(id)))) +
               "close"_kw};
       const auto skipper = SkipperBuilder().ignore(ws).build();
       expect_cache_neutral(
           [&](const ParseOptions &opts) {
             return parseRule(rule, input, skipper, opts);
           },
           name, input);
     },
     "open gamma name close"},
};

} // namespace

TEST(RecoveryCacheNeutrality, cache_neutral_across_modalities) {
  for (const auto &c : kCases) {
    SCOPED_TRACE(c.name);
    c.build(c.name, c.input);
  }
}
