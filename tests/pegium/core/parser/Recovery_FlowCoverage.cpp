// Flow-coverage tests for recovery helpers that previously had zero/low
// direct unit coverage, plus two targeted regression tests for fixes that
// just landed:
//   * #1 cyclic-grammar no-crash in
//     `entry_rule_has_unguarded_leading_visible_entry` (depth-guard regression)
//   * #9 multibyte resync skip budget consumed per-CODEPOINT
//     (`maxResyncSkipCodepoints` bytes-vs-codepoints contract regression)
//
// Tests-only; src/ is never touched. Every case follows the suite's
// table-driven + SCOPED_TRACE style (no TEST_P).

#include <gtest/gtest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <pegium/core/RecoveryHarnessTestSupport.hpp>
#include <pegium/core/grammar/AbstractElement.hpp>
#include <pegium/core/grammar/Group.hpp>
#include <pegium/core/grammar/OrderedChoice.hpp>
#include <pegium/core/grammar/ParserRule.hpp>
#include <pegium/core/grammar/Repetition.hpp>
#include <pegium/core/grammar/UnorderedGroup.hpp>
#include <pegium/core/parser/LiteralFuzzyMatcher.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/ParseDiagnostics.hpp>
#include <pegium/core/parser/RecoveryAnalysis.hpp>
#include <pegium/core/parser/RecoverySearch.hpp>

#include "RecoveryTestSupport.hpp"

using namespace pegium::parser;
using pegium::test::recovery::dump_parse_diagnostics;
using pegium::test::recovery::parseRule;

namespace {

// ---------------------------------------------------------------------------
// Minimal hand-built grammar elements.
//
// The leading-visible-entry classifier (`classify_leading_visible_entry`,
// reached via `entry_rule_has_unguarded_leading_visible_entry`) is purely
// grammar-structural: it switches on `getKind()` and recurses through
// `getElement()` / `get(index)`. Building fake elements directly (mirroring
// `RecursivePrintRule`/`RecursivePrintGroup` in Parser.cpp) gives precise
// control over every branch / shape without fighting the `ParserRule<T>`
// non-nullable-body constructor constraint, and lets us wire a true cycle.
// These objects never parse anything; only the static classifier reads them.
// ---------------------------------------------------------------------------

// A bare visible terminal: the classifier returns `Unguarded` for `Literal`.
struct FakeLiteral final : pegium::grammar::AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::Literal;
  }
  constexpr bool isNullable() const noexcept override { return false; }
  void print(std::ostream &os) const override { os << "lit"; }
};

// A positive-lookahead predicate guard: classifier returns `PredicateGuarded`.
struct FakeAndPredicate final : pegium::grammar::AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::AndPredicate;
  }
  constexpr bool isNullable() const noexcept override { return true; }
  void print(std::ostream &os) const override { os << "&pred"; }
};

// A negative-lookahead predicate guard: classifier returns `PredicateGuarded`.
struct FakeNotPredicate final : pegium::grammar::AbstractElement {
  constexpr ElementKind getKind() const noexcept override {
    return ElementKind::NotPredicate;
  }
  constexpr bool isNullable() const noexcept override { return true; }
  void print(std::ostream &os) const override { os << "!pred"; }
};

// A Repetition with configurable `min`. `min == 0` makes the leading edge
// optional, so the classifier returns `None`.
struct FakeRepetition final : pegium::grammar::Repetition {
  const pegium::grammar::AbstractElement *child = nullptr;
  std::size_t min = 0;
  std::size_t max = 1;
  bool nullableFlag = true;

  constexpr bool isNullable() const noexcept override { return nullableFlag; }
  std::size_t getMin() const noexcept override { return min; }
  std::size_t getMax() const noexcept override { return max; }
  const pegium::grammar::AbstractElement *getElement() const noexcept override {
    return child;
  }
};

// An OrderedChoice over a fixed set of branch pointers.
struct FakeOrderedChoice final : pegium::grammar::OrderedChoice {
  std::vector<const pegium::grammar::AbstractElement *> branches;
  bool nullableFlag = false;

  constexpr bool isNullable() const noexcept override { return nullableFlag; }
  const pegium::grammar::AbstractElement *
  get(std::size_t index) const noexcept override {
    return index < branches.size() ? branches[index] : nullptr;
  }
  std::size_t size() const noexcept override { return branches.size(); }
};

// An UnorderedGroup over a fixed set of member pointers.
struct FakeUnorderedGroup final : pegium::grammar::UnorderedGroup {
  std::vector<const pegium::grammar::AbstractElement *> members;
  bool nullableFlag = false;

  constexpr bool isNullable() const noexcept override { return nullableFlag; }
  const pegium::grammar::AbstractElement *
  get(std::size_t index) const noexcept override {
    return index < members.size() ? members[index] : nullptr;
  }
  std::size_t size() const noexcept override { return members.size(); }
};

// A Group whose single child is the leading edge (used to wire the cycle).
struct FakeGroup final : pegium::grammar::Group {
  const pegium::grammar::AbstractElement *child = nullptr;

  constexpr bool isNullable() const noexcept override { return false; }
  const pegium::grammar::AbstractElement *
  get(std::size_t index) const noexcept override {
    return index == 0 ? child : nullptr;
  }
  std::size_t size() const noexcept override { return 1; }
};

// A minimal concrete `grammar::ParserRule` whose body element is a swappable
// pointer. Only the structural overrides used by the classifier are real; the
// parse/value overrides are inert (never invoked by the static classifier).
struct FakeParserRule final : pegium::grammar::ParserRule {
  const pegium::grammar::AbstractElement *child = nullptr;
  ElementKind kind = ElementKind::ParserRule;

  constexpr ElementKind getKind() const noexcept override { return kind; }
  constexpr bool isNullable() const noexcept override { return false; }
  std::string_view getTypeName() const noexcept override { return "Fake"; }
  std::string_view getName() const noexcept override { return "Fake"; }

  pegium::AstNode *getValue(const pegium::CstNodeView &,
                            const ValueBuildContext &) const override {
    return nullptr;
  }
  bool rule(ParseContext &) const override { return false; }
  bool rule(TrackedParseContext &) const override { return false; }
  bool recover(RecoveryContext &) const override { return false; }
  bool expect(ExpectContext &) const override { return false; }
  void init(AstReflectionInitContext &) const override {}
  const pegium::grammar::AbstractElement *getElement() const noexcept override {
    return child;
  }
};

} // namespace

// ===========================================================================
// Target #1 — entry_rule_has_unguarded_leading_visible_entry
// ===========================================================================

// CRITICAL regression: a grammar whose entry rule references itself through
// rule indirection used to drive `classify_leading_visible_entry` into
// infinite recursion (stack overflow). A depth guard
// (kMaxLeadingVisibleEntryDepth == 256) now bounds the walk. We build a true
// cycle `a -> Group -> a` and assert the classifier simply RETURNS without
// crashing/hanging. We never PARSE with this grammar (it is left-recursive
// and parsing would loop); only the static classifier is invoked.
//
// Exact construction (mirrors RecursivePrintRule/Group in Parser.cpp):
//   FakeParserRule a;      a.child = &group;   // a's body is `group`
//   FakeGroup      group;  group.child = &a;   // group's sole child is `a`
// so `a.getElement() == &group`, `group.get(0) == &a`, forming the cycle
// a -> group -> a -> group -> ...  The classifier walks
// ParserRule(a) -> Group -> child ParserRule(a) -> Group -> ... until the
// depth cap returns the conservative `Unguarded` default (== returns true).
TEST(RecoveryFlowCoverageTest, CyclicEntryRuleClassifierTerminatesWithoutCrash) {
  FakeParserRule a;
  FakeGroup group;
  a.child = std::addressof(group);
  group.child = std::addressof(a);

  // The contract is "it returns" (no infinite recursion / stack overflow).
  // The depth-cap fallback classifies the bottomed-out edge as Unguarded, so
  // a non-PredicateGuarded cycle resolves to `true`. Asserting the concrete
  // value also pins that the guard returns the conservative default.
  const bool classified =
      detail::entry_rule_has_unguarded_leading_visible_entry(a);
  EXPECT_TRUE(classified);
}

// Deep-but-acyclic chain that, without the depth guard, would also overflow:
// 300 nested single-child Groups terminating in a Literal. With the cap the
// walk bottoms out at depth 256 and returns the Unguarded default.
TEST(RecoveryFlowCoverageTest,
     DeepAcyclicEntryRuleChainTerminatesWithoutCrash) {
  constexpr std::size_t kChainDepth = 300;
  FakeLiteral leaf;
  std::vector<FakeGroup> groups(kChainDepth);
  for (std::size_t i = 0; i < kChainDepth; ++i) {
    groups[i].child = (i + 1 < kChainDepth)
                          ? static_cast<const pegium::grammar::AbstractElement *>(
                                std::addressof(groups[i + 1]))
                          : static_cast<const pegium::grammar::AbstractElement *>(
                                std::addressof(leaf));
  }
  FakeParserRule entry;
  entry.child = std::addressof(groups.front());

  EXPECT_TRUE(detail::entry_rule_has_unguarded_leading_visible_entry(entry));
}

// Normal (non-pathological) classifier outcomes over shapes that were
// previously uncovered. `entry_rule_has_unguarded_leading_visible_entry`
// returns `kind != PredicateGuarded`, i.e. true for both Unguarded and None
// leading edges, false only for a PredicateGuarded leading edge.
TEST(RecoveryFlowCoverageTest, ClassifierShapeOutcomes) {
  // Shared leaf elements referenced by the shapes below; kept alive for the
  // whole test body.
  FakeLiteral literal;
  FakeAndPredicate andPredicate;
  FakeNotPredicate notPredicate;

  // Shape A: entry body is a plain visible literal -> Unguarded -> true.
  FakeParserRule plainLiteralRule;
  plainLiteralRule.child = std::addressof(literal);

  // Shape B: entry body is a Repetition with min == 0 -> None -> true.
  FakeRepetition optionalRep;
  optionalRep.min = 0;
  optionalRep.max = 1;
  optionalRep.child = std::addressof(literal);
  FakeParserRule optionalRule;
  optionalRule.child = std::addressof(optionalRep);

  // Shape C: entry body is a Repetition with min >= 1 over a literal ->
  // Unguarded -> true.
  FakeRepetition someRep;
  someRep.min = 1;
  someRep.max = 8;
  someRep.nullableFlag = false;
  someRep.child = std::addressof(literal);
  FakeParserRule someRule;
  someRule.child = std::addressof(someRep);

  // Shape D: entry body is an UnorderedGroup containing a literal -> the
  // unguarded member wins -> Unguarded -> true.
  FakeUnorderedGroup unordered;
  unordered.members = {std::addressof(andPredicate), std::addressof(literal)};
  FakeParserRule unorderedRule;
  unorderedRule.child = std::addressof(unordered);

  // Shape E: entry body is an OrderedChoice ALL of whose branches are
  // predicate-guarded -> PredicateGuarded -> false (not "unguarded").
  FakeOrderedChoice guardedChoice;
  guardedChoice.branches = {std::addressof(andPredicate),
                            std::addressof(notPredicate)};
  FakeParserRule guardedChoiceRule;
  guardedChoiceRule.child = std::addressof(guardedChoice);

  // Shape F: an OrderedChoice with at least one unguarded (literal) branch
  // -> Unguarded -> true (control row, proves E's guard is load-bearing).
  FakeOrderedChoice mixedChoice;
  mixedChoice.branches = {std::addressof(andPredicate), std::addressof(literal)};
  FakeParserRule mixedChoiceRule;
  mixedChoiceRule.child = std::addressof(mixedChoice);

  struct Case {
    const char *label;
    const pegium::grammar::ParserRule *rule;
    bool expectedUnguarded;
  };
  const Case cases[] = {
      {"plain literal entry -> unguarded", &plainLiteralRule, true},
      {"repetition min==0 -> None -> not predicate-guarded", &optionalRule,
       true},
      {"repetition min>=1 over literal -> unguarded", &someRule, true},
      {"unordered group with literal member -> unguarded", &unorderedRule,
       true},
      {"ordered choice all branches predicate-guarded -> not unguarded",
       &guardedChoiceRule, false},
      {"ordered choice with one literal branch -> unguarded", &mixedChoiceRule,
       true},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);
    EXPECT_EQ(detail::entry_rule_has_unguarded_leading_visible_entry(*c.rule),
              c.expectedUnguarded);
  }
}

// Non-ParserRule entries are classified `false` by contract. We build a fake
// rule whose reported kind is NOT ParserRule to exercise the early-return.
TEST(RecoveryFlowCoverageTest, NonParserRuleEntryIsNeverUnguarded) {
  FakeLiteral literal;
  FakeParserRule notARule;
  notARule.child = std::addressof(literal);
  notARule.kind = pegium::grammar::ElementKind::DataTypeRule;

  EXPECT_FALSE(detail::entry_rule_has_unguarded_leading_visible_entry(notARule));
}

// ===========================================================================
// Target #2 — multibyte resync skip budget (#9 bytes-vs-codepoints contract)
// ===========================================================================

// A `Repetition` panic-mode resync skips noise one CODEPOINT at a time, with
// the budget measured in codepoints despite `maxResyncSkipCodepoints`'s name. We
// feed multibyte (UTF-8, 3-byte CJK) garbage before the first recoverable
// iteration element and assert the fused `Delete` span spans MORE BYTES than
// the codepoint budget value — the regression guard for #9.
TEST(RecoveryFlowCoverageTest, MultibyteResyncSkipBudgetIsCodepointCounted) {
  using namespace pegium::test::recovery;
  const auto skipper = SkipperBuilder().ignore(some(s)).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // A list whose element is `req <id>`. Leading garbage forces the iteration
  // into panic-mode resync to find the first clean `req`.
  ParserRule<RecoveryRequirementNode> requirement{
      "Requirement", "req"_kw + assign<&RecoveryRequirementNode::name>(id)};
  ParserRule<RecoveryRequirementModelNode> model{
      "RequirementModel",
      some(append<&RecoveryRequirementModelNode::requirements>(requirement))};

  // 12 CJK codepoints (each 3 bytes in UTF-8) == 36 bytes of garbage, then a
  // clean requirement. With a codepoint budget of >=12 the whole garbage run
  // is deletable; a byte budget of 12 would have stopped at byte 12 (mid
  // codepoint, 4 codepoints) and failed to reach `req`.
  const std::string garbage =
      "\xE4\xB8\x80\xE4\xB8\x81\xE4\xB8\x82\xE4\xB8\x83" // 4 CJK
      "\xE4\xB8\x84\xE4\xB8\x85\xE4\xB8\x86\xE4\xB8\x87" // 4 CJK
      "\xE4\xB8\x88\xE4\xB8\x89\xE4\xB8\x8A\xE4\xB8\x8B"; // 4 CJK
  const std::size_t kGarbageCodepoints = 12;
  const std::size_t kGarbageBytes = garbage.size();
  ASSERT_EQ(kGarbageBytes, kGarbageCodepoints * 3u);
  const std::string input = garbage + "\nreq login\n";

  ParseOptions options;
  // Budget in codepoints: large enough to span all 12 garbage codepoints, but
  // strictly SMALLER than the garbage byte count, so a byte-counted budget of
  // the same value could not have skipped the whole multibyte run.
  options.maxResyncSkipCodepoints = 16;
  ASSERT_LT(options.maxResyncSkipCodepoints, kGarbageBytes);
  ASSERT_GE(options.maxResyncSkipCodepoints, kGarbageCodepoints);

  const auto result = parseRule(model, input, skipper, options);
  const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

  ASSERT_TRUE(result.value) << parseDump;
  EXPECT_TRUE(result.fullMatch) << parseDump;

  // Locate the fused Delete that begins at offset 0 (start of the garbage).
  const ParseDiagnostic *deletion = nullptr;
  for (const auto &diagnostic : result.parseDiagnostics) {
    if (diagnostic.kind == ParseDiagnosticKind::Deleted &&
        diagnostic.beginOffset == 0u) {
      deletion = std::addressof(diagnostic);
      break;
    }
  }
  ASSERT_NE(deletion, nullptr) << parseDump;

  const auto deletedBytes = deletion->endOffset - deletion->beginOffset;
  // The deleted span covers all multibyte garbage: its byte length exceeds the
  // numeric budget value — the codepoint-counted contract in action.
  EXPECT_GE(deletedBytes, static_cast<pegium::TextOffset>(kGarbageBytes))
      << parseDump;
  EXPECT_GT(deletedBytes,
            static_cast<pegium::TextOffset>(options.maxResyncSkipCodepoints))
      << parseDump;

  // The clean requirement after the garbage was recovered.
  auto *parsedModel =
      dynamic_cast<RecoveryRequirementModelNode *>(result.value);
  ASSERT_NE(parsedModel, nullptr) << parseDump;
  ASSERT_EQ(parsedModel->requirements.size(), 1u) << parseDump;
  EXPECT_EQ(parsedModel->requirements[0]->name, "login") << parseDump;
}

// ===========================================================================
// Target #3 — literal_has_single_edit_strict_match
// ===========================================================================

// Faithful DP oracle for the strict O(N) detector: the header documents it as
// equivalent to `find_best_literal_fuzzy_candidate(...).distance == 1 &&
// |consumed-N| <= 1`. Because the strict matcher checks PREFIXES of the window
// (lengths N, N+1, N-1), the equivalent DP truth must scan ALL candidates'
// `consumed` prefixes (find_literal_fuzzy_candidates), not just the best
// whole-window candidate — `consumed` is exactly the prefix length each
// candidate matched.
static bool dp_has_single_edit_prefix(std::string_view literal,
                                      std::string_view window,
                                      bool caseSensitive) {
  detail::LiteralFuzzyCandidatesCache cache;
  const auto candidates = detail::find_literal_fuzzy_candidates_view(
      literal, window, caseSensitive, cache);
  const auto literalLen = static_cast<long>(literal.size());
  for (const auto &candidate : candidates) {
    if (candidate.distance != 1u) {
      continue;
    }
    if (std::abs(static_cast<long>(candidate.consumed) - literalLen) <= 1) {
      return true;
    }
  }
  return false;
}

// For the cases the strict detector was designed to mirror, it agrees with the
// DP probe: every single-edit kind (substitution / insertion / deletion /
// transposition) matches, while a 2-edit window, an empty literal (N==0) and
// an empty window all reject — and both functions concur on each.
TEST(RecoveryFlowCoverageTest, SingleEditStrictMatchAgreesWithDpProbe) {
  struct Case {
    const char *label;
    std::string_view literal;
    std::string_view window;
    bool caseSensitive;
  };
  const Case cases[] = {
      {"substitution", "service", "servixe", true},
      {"insertion in window (window longer by 1)", "module", "modulee", true},
      {"deletion from window (window shorter by 1)", "module", "modle", true},
      {"adjacent transposition", "module", "modlue", true},
      {"two-edit case rejected", "service", "sxrivxe", true},
      {"empty literal N==0 rejected", "", "abc", true},
      {"empty window rejected", "module", "", true},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);
    const bool dpSingleEdit =
        dp_has_single_edit_prefix(c.literal, c.window, c.caseSensitive);
    const bool strictMatch = detail::literal_has_single_edit_strict_match(
        c.literal, c.window, c.caseSensitive);
    EXPECT_EQ(strictMatch, dpSingleEdit);
  }
}

// Standalone documented contract of the O(N) prefix detector for shapes where
// it intentionally DIVERGES from the anchor-pruned DP best-candidate (so the
// agreement oracle above is not the right tool): it inspects only the window
// PREFIXES of length N, N+1, N-1 and rejects when even the shortest probed
// prefix is wider than the window (K > W). These rows assert the strict
// function's own value directly.
TEST(RecoveryFlowCoverageTest, SingleEditStrictMatchPrefixContract) {
  struct Case {
    const char *label;
    std::string_view literal;
    std::string_view window;
    bool caseSensitive;
    bool expected;
  };
  const Case cases[] = {
      // N+1 window prefix is a single insertion even though the rest of the
      // window is extra — strict matches the prefix.
      {"window wider than literal by 2 -> N+1 prefix single insert -> true",
       "ab", "abcd", true, true},
      // Exact match: the N-1 prefix ("modul") is literal-minus-last-char, a
      // single deletion alignment -> strict reports a match.
      {"exact match via N-1 prefix alignment -> true", "module", "module", true,
       true},
      // Window narrower than N-1: every probed prefix length K (N, N+1, N-1)
      // exceeds W, so all alignments are rejected (the K>W guard).
      {"window narrower than N-1 (K>W on every alignment) -> false", "module",
       "mo", true, false},
      // Case-insensitive exact match still matches via the prefix alignment.
      {"case-insensitive exact match -> true", "MoDuLe", "module", false, true},
      // Case-sensitive: multiple letters differ -> more than one edit -> false.
      {"case-sensitive multi-substitution -> false", "MoDuLe", "module", true,
       false},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);
    EXPECT_EQ(detail::literal_has_single_edit_strict_match(c.literal, c.window,
                                                          c.caseSensitive),
              c.expected);
  }
}

// ===========================================================================
// Target #4 — classify_recovery_attempt demotion arms +
//             should_fallback_to_parsed_length_snapshot
// ===========================================================================

namespace {

// Build the edit script (and keep cached facts in sync, mirroring
// set_recovery_edits in RecoverySearch.cpp).
void setEdits(detail::RecoveryAttempt &attempt,
              std::vector<detail::SyntaxScriptEntry> edits) {
  attempt.recoveryEdits = std::move(edits);
  attempt.facts = detail::derive_attempt_facts(attempt);
}

detail::SyntaxScriptEntry deleteEdit(pegium::TextOffset begin,
                                     pegium::TextOffset end) {
  return {.kind = ParseDiagnosticKind::Deleted,
          .offset = begin,
          .beginOffset = begin,
          .endOffset = end,
          .element = nullptr,
          .message = {}};
}

detail::SyntaxScriptEntry insertEdit(pegium::TextOffset at) {
  return {.kind = ParseDiagnosticKind::Inserted,
          .offset = at,
          .beginOffset = at,
          .endOffset = at,
          .element = nullptr,
          .message = {}};
}

} // namespace

// The two cited uncovered demotion arms inside the `fullMatch ||
// stableAfterRecovery` block:
//   arm 1: !fullMatch && hasEditPastReplayWindowHorizon -> RecoveredButNotCredible
//   arm 2: deleteOnlyWithoutContinuation              -> RecoveredButNotCredible
// plus the credible (Selectable) control that bypasses both.
TEST(RecoveryFlowCoverageTest, ClassifyDemotionArms) {
  struct Case {
    const char *label;
    bool fullMatch;
    bool stableAfterRecovery;
    bool hasStablePrefix;
    pegium::TextOffset stablePrefixOffset;
    pegium::TextOffset parsedLength;
    pegium::TextOffset maxCursorOffset;
    std::optional<detail::RecoveryWindow> replayWindow;
    std::vector<detail::SyntaxScriptEntry> edits;
    detail::RecoveryAttemptStatus expected;
  };

  const auto makeWindow = [](pegium::TextOffset maxCursor) {
    detail::RecoveryWindow window;
    window.maxCursorOffset = maxCursor;
    return window;
  };

  std::vector<Case> cases;

  // arm 1: stable-after-recovery (not a full match) whose LAST edit lands past
  // the replay window horizon -> demoted to RecoveredButNotCredible.
  cases.push_back(
      {"stable-after-recovery with edit past replay horizon -> not credible",
       /*fullMatch=*/false, /*stableAfterRecovery=*/true,
       /*hasStablePrefix=*/false, /*stablePrefixOffset=*/0,
       /*parsedLength=*/30, /*maxCursorOffset=*/30,
       /*replayWindow=*/makeWindow(10),
       /*edits=*/{insertEdit(20)},
       detail::RecoveryAttemptStatus::RecoveredButNotCredible});

  // arm 2: stable-after-recovery, delete-only, with NO continuation past the
  // last edit (parsedLength <= lastEditOffset) -> RecoveredButNotCredible.
  // No replay window so arm 1 cannot fire first.
  cases.push_back(
      {"stable-after-recovery delete-only without continuation -> not credible",
       /*fullMatch=*/false, /*stableAfterRecovery=*/true,
       /*hasStablePrefix=*/false, /*stablePrefixOffset=*/0,
       /*parsedLength=*/12, /*maxCursorOffset=*/12,
       /*replayWindow=*/std::nullopt,
       /*edits=*/{deleteEdit(8, 12)},
       detail::RecoveryAttemptStatus::RecoveredButNotCredible});

  // control: full match, delete-only but WITH continuation past the last edit
  // and edit inside the replay horizon -> stays Selectable (bypasses both
  // arms), proving the demotions above are load-bearing.
  cases.push_back(
      {"full match delete-only with continuation -> selectable",
       /*fullMatch=*/true, /*stableAfterRecovery=*/false,
       /*hasStablePrefix=*/false, /*stablePrefixOffset=*/0,
       /*parsedLength=*/20, /*maxCursorOffset=*/20,
       /*replayWindow=*/makeWindow(20),
       /*edits=*/{deleteEdit(8, 12)},
       detail::RecoveryAttemptStatus::Selectable});

  for (auto &c : cases) {
    SCOPED_TRACE(c.label);
    detail::RecoveryAttempt attempt;
    attempt.entryRuleMatched = true;
    attempt.fullMatch = c.fullMatch;
    attempt.stableAfterRecovery = c.stableAfterRecovery;
    attempt.reachedRecoveryTarget = true;
    attempt.hasStablePrefix = c.hasStablePrefix;
    attempt.stablePrefixOffset = c.stablePrefixOffset;
    attempt.parsedLength = c.parsedLength;
    attempt.maxCursorOffset = c.maxCursorOffset;
    attempt.replayWindow = c.replayWindow;
    // Generous budget so the early budget-overflow arm never fires here.
    attempt.configuredMaxEditCost = 1024;
    attempt.editCost = 1;
    setEdits(attempt, std::move(c.edits));

    detail::classify_recovery_attempt(attempt);
    EXPECT_EQ(attempt.status, c.expected);
  }
}

// The budget-overflow demotion arm: editCost > configuredMaxEditCost (and not
// a full match / stable-prefix / single-insert local-gap recovery) collapses
// to RecoveredButNotCredible when there are edits, or StrictFailure when there
// are none (the no-edit shape).
TEST(RecoveryFlowCoverageTest, ClassifyBudgetOverflowArms) {
  struct Case {
    const char *label;
    bool hasEdits;
    detail::RecoveryAttemptStatus expected;
  };
  const Case cases[] = {
      {"budget overflow with edits -> not credible", true,
       detail::RecoveryAttemptStatus::RecoveredButNotCredible},
      {"budget overflow without edits -> strict failure", false,
       detail::RecoveryAttemptStatus::StrictFailure},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);
    detail::RecoveryAttempt attempt;
    attempt.entryRuleMatched = true;
    attempt.fullMatch = false;
    attempt.reachedRecoveryTarget = false;
    attempt.hasStablePrefix = false;
    attempt.parsedLength = 4;
    attempt.maxCursorOffset = 4;
    attempt.configuredMaxEditCost = 1;
    attempt.editCost = 99; // strictly over budget
    if (c.hasEdits) {
      setEdits(attempt, {deleteEdit(0, 4)});
    } else {
      setEdits(attempt, {});
    }
    detail::classify_recovery_attempt(attempt);
    EXPECT_EQ(attempt.status, c.expected);
  }
}

// should_fallback_to_parsed_length_snapshot degenerate / underflow-guard arms.
// Returns true only when: parsedLength != 0, some leaf ends at-or-before
// parsedLength, hasFailureToken, the failure leaf begins past parsedLength,
// failureTokenIndex != 0 (the underflow guard), and the previous leaf is
// contiguous with and entirely past parsedLength.
TEST(RecoveryFlowCoverageTest, ShouldFallbackToParsedLengthSnapshotArms) {
  using detail::FailureLeaf;
  using detail::FailureSnapshot;

  struct Case {
    const char *label;
    std::vector<FailureLeaf> leaves;
    std::size_t failureTokenIndex;
    bool hasFailureToken;
    pegium::TextOffset parsedLength;
    bool expected;
  };

  const std::vector<Case> cases = {
      // parsedLength == 0 -> false (degenerate).
      {"parsedLength==0 short-circuits to false",
       {{.beginOffset = 0, .endOffset = 5, .element = nullptr}},
       /*failureTokenIndex=*/0, /*hasFailureToken=*/true,
       /*parsedLength=*/0, /*expected=*/false},
      // No leaf ends at-or-before parsedLength -> false.
      {"no leaf ends within parsedLength -> false",
       {{.beginOffset = 10, .endOffset = 15, .element = nullptr}},
       /*failureTokenIndex=*/0, /*hasFailureToken=*/true,
       /*parsedLength=*/5, /*expected=*/false},
      // hasFailureToken == false -> false.
      {"no failure token -> false",
       {{.beginOffset = 0, .endOffset = 5, .element = nullptr},
        {.beginOffset = 6, .endOffset = 11, .element = nullptr}},
       /*failureTokenIndex=*/1, /*hasFailureToken=*/false,
       /*parsedLength=*/5, /*expected=*/false},
      // failureTokenIndex == 0 underflow guard -> false (the failure leaf
      // begins past parsedLength but there is no previous leaf to inspect).
      {"failureTokenIndex==0 underflow guard -> false",
       {{.beginOffset = 6, .endOffset = 11, .element = nullptr}},
       /*failureTokenIndex=*/0, /*hasFailureToken=*/true,
       /*parsedLength=*/5, /*expected=*/false},
      // failure leaf begins at/within parsedLength -> false.
      {"failure leaf begins within parsedLength -> false",
       {{.beginOffset = 0, .endOffset = 5, .element = nullptr},
        {.beginOffset = 5, .endOffset = 11, .element = nullptr}},
       /*failureTokenIndex=*/1, /*hasFailureToken=*/true,
       /*parsedLength=*/5, /*expected=*/false},
      // Previous leaf NOT contiguous with the failure leaf -> false.
      {"previous leaf non-contiguous -> false",
       {{.beginOffset = 0, .endOffset = 3, .element = nullptr},
        {.beginOffset = 6, .endOffset = 9, .element = nullptr},
        {.beginOffset = 10, .endOffset = 15, .element = nullptr}},
       /*failureTokenIndex=*/2, /*hasFailureToken=*/true,
       /*parsedLength=*/3, /*expected=*/false},
      // All conditions satisfied -> true. parsedLength == 2: leaf[0] ends at 2
      // (<= parsedLength), failure leaf[2] begins at 10 (> 2), previous leaf[1]
      // ends at 10 (contiguous) and begins at 6 (> 2).
      {"all conditions satisfied -> true",
       {{.beginOffset = 0, .endOffset = 2, .element = nullptr},
        {.beginOffset = 6, .endOffset = 10, .element = nullptr},
        {.beginOffset = 10, .endOffset = 15, .element = nullptr}},
       /*failureTokenIndex=*/2, /*hasFailureToken=*/true,
       /*parsedLength=*/2, /*expected=*/true},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);
    FailureSnapshot snapshot;
    snapshot.failureLeafHistory = c.leaves;
    snapshot.failureTokenIndex = c.failureTokenIndex;
    snapshot.hasFailureToken = c.hasFailureToken;
    EXPECT_EQ(detail::should_fallback_to_parsed_length_snapshot(snapshot,
                                                               c.parsedLength),
              c.expected);
  }
}

// ===========================================================================
// Target #5 — out-of-window single-fuzzy-edit keyword-typo carve-out
// ===========================================================================

// PINS the just-landed capability: a keyword typo'd OUTSIDE the active recovery
// window (i.e. after an EARLIER statement already committed a recovery edit and
// advanced the window past it) now recovers as a single `Replaced` edit. The
// carve-out gates on the Damerau-Levenshtein operation distance == 1 instead of
// the weighted `replacementCost <= 1`, which is exactly what unlocks the two
// classes pinned here:
//   * TRANSPOSITION (`modlue` -> `module`): weighted cost 2, one operation.
//   * EXTRA CODEPOINT (`modulee` -> `module`): weighted cost 2, one operation.
// (The MISSING-codepoint class already recovered before this change, and the
// SUBSTITUTION class is independently blocked by the pre-existing
// `triviaGap.hasHiddenGap && substitutionCount != 0` filter, so neither is
// asserted here.)
//
// Scenario (mirrors the validating prototype): a `some(...)` list of
// `"module" <id>` statements. The FIRST keyword is itself typo'd (`modle`, a
// missing-codepoint repair) so it commits a Replace at 0-5 and opens/advances
// the recovery window. The SECOND keyword — now out of that window — is typo'd
// with the class under test; with the carve-out it folds into a single
// `Replaced` covering the typo'd keyword span, and the parse reaches the second
// statement (`beta`).
//
// The carve-out rides on an INTERNAL probe axis
// (`RecoveryAttemptSpec::probeAxes.forbidOutOfWindowFuzzyFold`, set only by the
// no-fold sibling probe), not a public ParseOptions flag, so it cannot be forced
// off through this full-pipeline entry point; that the carve-out is load-bearing
// is regression-locked by RecoveryRankingBaselineTest.ChosenCandidateMatchesGolden
// (which flips to a less-parsimonious delete+Replace if the no-fold probe is off).
TEST(RecoveryFlowCoverageTest, OutOfWindowSingleFuzzyTypoKeywordRecovers) {
  using namespace pegium::test::recovery;
  const auto skipper = SkipperBuilder().ignore(some(s)).build();

  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  // Each statement starts with the word keyword "module"; the list is a
  // `some(...)` of them so a later statement's keyword lands past the window the
  // first statement's committed recovery opens.
  ParserRule<RecoveryModule> statement{
      "Statement", "module"_kw + assign<&RecoveryModule::name>(id)};
  ParserRule<RecoveryModule> root{
      "Root", some(append<&RecoveryModule::statements>(statement))};

  struct Case {
    const char *label;
    std::string_view input;
    // [begin,end) byte span of the typo'd SECOND keyword that must recover as a
    // single Replaced edit under the carve-out.
    pegium::TextOffset secondKeywordBegin;
    pegium::TextOffset secondKeywordEnd;
  };
  // First keyword "modle" (missing codepoint) commits a Replace at 0-5 and
  // advances the window; layout: "modle"(0-4) " "(5) "alpha"(6-10) " "(11)
  // then the typo'd second keyword starting at offset 12.
  const Case cases[] = {
      // Transposition: "modlue" (6 bytes) -> "module", span 12-18.
      {"transposition out-of-window", "modle alpha modlue beta", 12, 18},
      // Extra codepoint: "modulee" (7 bytes) -> "module", span 12-19.
      {"extra-codepoint out-of-window", "modle alpha modulee beta", 12, 19},
  };

  for (const auto &c : cases) {
    SCOPED_TRACE(c.label);

    // --- Positive leg: carve-out ON (default). The second keyword recovers as
    // a single Replaced edit and the parse reaches both statements.
    {
      const auto result = parseRule(root, c.input, skipper);
      const auto parseDump = dump_parse_diagnostics(result.parseDiagnostics);

      ASSERT_TRUE(result.value) << parseDump;
      EXPECT_TRUE(result.fullMatch) << parseDump;

      const bool firstKeywordReplaced = std::ranges::any_of(
          result.parseDiagnostics, [](const ParseDiagnostic &diagnostic) {
            return diagnostic.kind == ParseDiagnosticKind::Replaced &&
                   diagnostic.beginOffset == 0u;
          });
      EXPECT_TRUE(firstKeywordReplaced) << parseDump;

      // The load-bearing assertion: the OUT-OF-WINDOW second keyword recovered
      // as one Replaced edit spanning exactly the typo'd keyword.
      const auto secondKeywordReplaced = std::ranges::find_if(
          result.parseDiagnostics, [&](const ParseDiagnostic &diagnostic) {
            return diagnostic.kind == ParseDiagnosticKind::Replaced &&
                   diagnostic.beginOffset == c.secondKeywordBegin;
          });
      ASSERT_NE(secondKeywordReplaced, result.parseDiagnostics.end())
          << parseDump;
      EXPECT_EQ(secondKeywordReplaced->endOffset, c.secondKeywordEnd)
          << parseDump;

      // Recovering the second keyword is what lets the list reach the second
      // statement; both names are present.
      auto *parsedRoot = dynamic_cast<RecoveryModule *>(result.value);
      ASSERT_NE(parsedRoot, nullptr) << parseDump;
      ASSERT_EQ(parsedRoot->statements.size(), 2u) << parseDump;
      auto *first = dynamic_cast<RecoveryModule *>(parsedRoot->statements[0]);
      auto *second = dynamic_cast<RecoveryModule *>(parsedRoot->statements[1]);
      ASSERT_NE(first, nullptr) << parseDump;
      ASSERT_NE(second, nullptr) << parseDump;
      EXPECT_EQ(first->name, "alpha") << parseDump;
      EXPECT_EQ(second->name, "beta") << parseDump;
    }

    // No negative leg here: the out-of-window carve-out is now driven by an
    // INTERNAL probe axis (`RecoveryAttemptSpec::probeAxes.forbidOutOfWindowFuzzyFold`,
    // set only by the no-fold sibling probe), not a public ParseOptions flag, so
    // it can no longer be forced off through this full-pipeline entry point. That
    // the carve-out is load-bearing is regression-locked by the ranking golden
    // oracle (RecoveryRankingBaselineTest.ChosenCandidateMatchesGolden flips to a
    // less-parsimonious delete+Replace if the no-fold probe is disabled).
  }
}
