#include <cstdint>
#include <functional>
#include <gtest/gtest.h>
#include <pegium/core/TestCstBuilderHarness.hpp>
#include <pegium/core/TestRuleParser.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <ranges>
#include <variant>

using namespace pegium::parser;

namespace {

enum class TerminalMode : std::uint16_t { Single = 42 };

template <typename T> struct TerminalValueNode : pegium::AstNode {
  T value{};
};

template <typename T> struct TerminalPairNode : pegium::AstNode {
  T first{};
  T second{};
};

template <typename T>
ParseResult parseTerminalRule(const TerminalRule<T> &rule, std::string_view text,
                              const Skipper &skipper,
                              const ParseOptions &options = {}) {
  ParserRule<TerminalValueNode<T>> root{
      "Root", assign<&TerminalValueNode<T>::value>(rule)};
  return pegium::test::parse_rule_result(root, text, skipper, options);
}

std::string dump_parse_diagnostics(
    const std::vector<ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    switch (diagnostic.kind) {
    case ParseDiagnosticKind::Inserted:
      dump += "Inserted";
      break;
    case ParseDiagnosticKind::Deleted:
      dump += "Deleted";
      break;
    case ParseDiagnosticKind::Replaced:
      dump += "Replaced";
      break;
    case ParseDiagnosticKind::Incomplete:
      dump += "Incomplete";
      break;
    case ParseDiagnosticKind::Recovered:
      dump += "Recovered";
      break;
    case ParseDiagnosticKind::ConversionError:
      dump += "ConversionError";
      break;
    }
    dump += "@";
    dump += std::to_string(diagnostic.beginOffset);
    dump += "-";
    dump += std::to_string(diagnostic.endOffset);
  }
  return dump;
}

} // namespace

TEST(TerminalRuleTest, ParseRequiresFullConsumption) {
  TerminalRule<std::string_view> terminal{"T", "hello"_kw};

  {
    auto result = parseTerminalRule(terminal, "hello", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }

  {
    auto result =
        parseTerminalRule(terminal, "helloX", SkipperBuilder().build());
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
    auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
        result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, "hello");
  }
}

TEST(TerminalRuleTest, IntegralConversionUsesFromChars) {
  TerminalRule<int> number{"Number", some(d)};

  {
    auto result = parseTerminalRule(number, "12345", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    EXPECT_TRUE(result.fullMatch);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, 12345);
  }

  {
    auto result =
        parseTerminalRule(number, "12345x", SkipperBuilder().build());
    EXPECT_FALSE(result.fullMatch);
    ASSERT_TRUE(result.value);
    ASSERT_EQ(result.parseDiagnostics.size(), 1u);
    EXPECT_EQ(result.parseDiagnostics.front().kind,
              ParseDiagnosticKind::Incomplete);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_EQ(typed->value, 12345);
  }
}

TEST(TerminalRuleTest, ParseRuleLeavesCursorAtTokenEnd) {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};

  auto builderHarness = pegium::test::makeCstBuilderHarness("abc   ");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().ignore(ws).build();

  ParseContext ctx{builder, skipper};
  auto result = parse(terminal, ctx);
  EXPECT_TRUE(result);
  EXPECT_EQ(ctx.cursor() - input.begin(), 3);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ((*it).getText(), "abc");
  ++it;
  EXPECT_EQ(it, root->end());
}

TEST(TerminalRuleTest, BoolConversionMapsTrueAndFalse) {
  TerminalRule<bool> flag{"Flag", "true"_kw | "false"_kw};

  auto yes = parseTerminalRule(flag, "true", SkipperBuilder().build());
  ASSERT_TRUE(yes.value);
  auto *yesTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(yes.value);
  ASSERT_TRUE(yesTyped != nullptr);
  EXPECT_TRUE(yesTyped->value);

  auto no = parseTerminalRule(flag, "false", SkipperBuilder().build());
  ASSERT_TRUE(no.value);
  auto *noTyped = pegium::ast_ptr_cast<TerminalValueNode<bool>>(no.value);
  ASSERT_TRUE(noTyped != nullptr);
  EXPECT_FALSE(noTyped->value);
}

TEST(TerminalRuleTest, FloatingPointConversionAndFailurePaths) {
  TerminalRule<double> number{"Number", some(dot)};

  {
    auto result = parseTerminalRule(number, "12.5", SkipperBuilder().build());
    ASSERT_TRUE(result.value);
    auto *typed =
        pegium::ast_ptr_cast<TerminalValueNode<double>>(result.value);
    ASSERT_TRUE(typed != nullptr);
    EXPECT_DOUBLE_EQ(typed->value, 12.5);
  }
}

TEST(TerminalRuleTest, CharRuleCanUseCustomConverter) {
  TerminalRule<char> ch{"Char", "x"_kw};

  ch.setValueConverter([](std::string_view sv) noexcept {
    return opt::conversion_value<char>(sv.empty() ? '\0' : sv.front());
  });

  auto result = parseTerminalRule(ch, "x", SkipperBuilder().build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<char>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 'x');
}

TEST(TerminalRuleTest, ConstructorOptionCanSetValueConverter) {
  TerminalRule<int> number{"Number", "42"_kw,
                           opt::with_converter([](std::string_view sv) noexcept {
                             return opt::conversion_value<int>(
                                 static_cast<int>(sv.size()) + 1);
                           })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.value);
  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 3);
}

TEST(TerminalRuleTest, ConverterCanReturnValueDirectly) {
  // Infallible converter: returns the value directly instead of wrapping it in
  // opt::conversion_value<...>. The conversion can never fail.
  TerminalRule<std::string> word{
      "Word", "hi"_kw,
      opt::with_converter(
          [](std::string_view sv) noexcept { return std::string(sv) + "!"; })};

  auto result = parseTerminalRule(word, "hi", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.parseDiagnostics.empty());
  auto *typed =
      pegium::ast_ptr_cast<TerminalValueNode<std::string>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, "hi!");
}

TEST(TerminalRuleTest, ConverterCanSetValue) {
  TerminalRule<int> number{
      "Number", "42"_kw,
      opt::with_converter(
          [](std::string_view sv) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_value<int>(static_cast<int>(sv.size()) + 2);
          })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  EXPECT_TRUE(result.parseDiagnostics.empty());

  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 4);
}

TEST(TerminalRuleTest,
     ConverterFailureProducesConversionDiagnosticAndFallbackValue) {
  TerminalRule<int> number{
      "Number", "42"_kw,
      opt::with_converter(
          [](std::string_view) noexcept -> opt::ConversionResult<int> {
            return opt::conversion_error<int>("bad number");
          })};

  auto result = parseTerminalRule(number, "42", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 1u);
  EXPECT_EQ(result.parseDiagnostics.front().kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics.front().message, "bad number");

  auto *typed = pegium::ast_ptr_cast<TerminalValueNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, 0);
}

TEST(TerminalRuleTest, MultipleConverterFailuresProduceOrderedDiagnostics) {
  TerminalRule<int> number{"Number", some(d),
                           opt::with_converter([](std::string_view) noexcept
                                                   -> opt::ConversionResult<int> {
                             return opt::conversion_error<int>("bad number");
                           })};
  ParserRule<TerminalPairNode<int>> root{
      "Root", assign<&TerminalPairNode<int>::first>(number) + " "_kw +
                  assign<&TerminalPairNode<int>::second>(number)};

  const auto result =
      pegium::test::parse_rule_result(root, "1 2", SkipperBuilder().build());
  ASSERT_TRUE(result.fullMatch);
  ASSERT_TRUE(result.value);
  ASSERT_EQ(result.parseDiagnostics.size(), 2u);
  EXPECT_EQ(result.parseDiagnostics[0].kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics[1].kind,
            ParseDiagnosticKind::ConversionError);
  EXPECT_EQ(result.parseDiagnostics[0].offset, 0u);
  EXPECT_EQ(result.parseDiagnostics[1].offset, 2u);
  EXPECT_EQ(result.parseDiagnostics[0].message, "bad number");
  EXPECT_EQ(result.parseDiagnostics[1].message, "bad number");

  auto *typed =
      pegium::ast_ptr_cast<TerminalPairNode<int>>(result.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->first, 0);
  EXPECT_EQ(typed->second, 0);
}

TEST(TerminalRuleTest, RecoveredLiteralBackedConverterUsesRecoveredLiteralValue) {
  TerminalRule<int> number{"Number", "1"_kw,
                           opt::with_converter([](std::string_view sv) noexcept
                                                   -> opt::ConversionResult<int> {
                             if (sv != "1") {
                               return opt::conversion_error<int>(
                                   "bad recovered number");
                             }
                             return opt::conversion_value<int>(1);
                           })};
  auto builderHarness = pegium::test::makeCstBuilderHarness("x");
  auto &builder = builderHarness.builder;
  builder.leaf(0, static_cast<pegium::TextOffset>(builder.getText().size()),
               std::addressof(number), false, true);

  auto root = builder.getRootCstNode();
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  const auto node = *it;
  ASSERT_TRUE(node.isRecovered());

  std::vector<ParseDiagnostic> diagnostics;
  const ValueBuildContext context{.diagnostics = &diagnostics};
  EXPECT_EQ(number.getRawValue(node, context), 1);
  EXPECT_TRUE(diagnostics.empty());
}

TEST(TerminalRuleTest, LiteralBackedTerminalRuleUsesLiteralReplaceRecoveryLocally) {
  TerminalRule<std::string> terminal{"Token", "service"_kw};

  auto builderHarness = pegium::test::makeCstBuilderHarness("servixe");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(terminal, ctx));
  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(ctx.snapshotRecoveryEdits()));
  const auto recoveryDump = dump_parse_diagnostics(diagnostics);
  EXPECT_TRUE(std::ranges::any_of(
      diagnostics, [](const ParseDiagnostic &diagnostic) {
        return diagnostic.kind == ParseDiagnosticKind::Replaced;
      })) << recoveryDump;
  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  EXPECT_EQ(terminal.getRawValue(*it), "service");
}

TEST(TerminalRuleTest, ParseRuleFailureLeavesCursorAndTreeUntouched) {
  TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
  auto builderHarness = pegium::test::makeCstBuilderHarness("abX");
  auto &builder = builderHarness.builder;
  const auto input = builder.getText();
  auto skipper = SkipperBuilder().build();

  ParseContext ctx{builder, skipper};
  auto result = parse(terminal, ctx);
  EXPECT_FALSE(result);
  EXPECT_EQ(ctx.cursor(), input.begin());

  auto root = builder.getRootCstNode();
  EXPECT_EQ(root->begin(), root->end());
}

TEST(TerminalRuleTest, NoInsertOverVisibleSourceOrEof) {
  // Folds LongLiteralBackedTerminalRuleDoesNotInsertAtEof,
  // WordLikeFreeFormTerminalDoesNotInsertSynthesizedLeafOverVisibleSource, and
  // CompositeWordLikeFreeFormTerminalDoesNotInsertSynthesizedLeafOverVisibleSource.
  // Each row builds a RecoveryContext over a TerminalRule<std::string_view> and
  // asserts the identical no-recovery + untouched-tree shape; only the terminal
  // builder and the harness input vary. Each row's `run` reproduces that test's
  // construction verbatim (some rows keep a backing `literal` terminal alive)
  // and returns the asserted scalars.
  struct NoRecovery {
    bool parsed;
    pegium::TextOffset cursorOffset;
    bool editsEmpty;
    bool rootNonNull;
    bool treeEmpty;
  };
  struct Case {
    const char *name;
    std::function<NoRecovery()> run;
  };
  static const Case kCases[] = {
      {"LongLiteralBackedTerminalRuleDoesNotInsertAtEof",
       [] {
         TerminalRule<std::string_view> terminal{"Token", "abc"_kw};
         auto builderHarness = pegium::test::makeCstBuilderHarness("");
         auto &builder = builderHarness.builder;
         auto skipper = SkipperBuilder().build();
         detail::FailureHistoryRecorder recorder(builder.input_begin());

         RecoveryContext ctx{builder, skipper, recorder};
         NoRecovery row;
         row.parsed = static_cast<bool>(parse(terminal, ctx));
         row.cursorOffset = ctx.cursorOffset();
         row.editsEmpty = ctx.snapshotRecoveryEdits().empty();
         const auto *root = builder.getRootCstNode();
         row.rootNonNull = root != nullptr;
         row.treeEmpty = root != nullptr && root->begin() == root->end();
         return row;
       }},
      {"WordLikeFreeFormTerminalDoesNotInsertSynthesizedLeafOverVisibleSource",
       [] {
         TerminalRule<std::string_view> literal{"Literal", "abc"_kw};
         TerminalRule<std::string_view> terminal{"Token", literal};
         auto builderHarness = pegium::test::makeCstBuilderHarness("?");
         auto &builder = builderHarness.builder;
         auto skipper = SkipperBuilder().build();
         detail::FailureHistoryRecorder recorder(builder.input_begin());

         RecoveryContext ctx{builder, skipper, recorder};
         NoRecovery row;
         row.parsed = static_cast<bool>(parse(terminal, ctx));
         row.cursorOffset = ctx.cursorOffset();
         row.editsEmpty = ctx.snapshotRecoveryEdits().empty();
         const auto *root = builder.getRootCstNode();
         row.rootNonNull = root != nullptr;
         row.treeEmpty = root != nullptr && root->begin() == root->end();
         return row;
       }},
      {"CompositeWordLikeFreeFormTerminalDoesNotInsertSynthesizedLeafOverVisibleSource",
       [] {
         TerminalRule<std::string_view> terminal{"Token",
                                                 "a-zA-Z_"_cr + many(w)};
         auto builderHarness = pegium::test::makeCstBuilderHarness("?");
         auto &builder = builderHarness.builder;
         auto skipper = SkipperBuilder().build();
         detail::FailureHistoryRecorder recorder(builder.input_begin());

         RecoveryContext ctx{builder, skipper, recorder};
         NoRecovery row;
         row.parsed = static_cast<bool>(parse(terminal, ctx));
         row.cursorOffset = ctx.cursorOffset();
         row.editsEmpty = ctx.snapshotRecoveryEdits().empty();
         const auto *root = builder.getRootCstNode();
         row.rootNonNull = root != nullptr;
         row.treeEmpty = root != nullptr && root->begin() == root->end();
         return row;
       }},
  };

  for (const auto &testCase : kCases) {
    SCOPED_TRACE(testCase.name);
    const auto row = testCase.run();
    EXPECT_FALSE(row.parsed);
    EXPECT_EQ(row.cursorOffset, 0u);
    EXPECT_TRUE(row.editsEmpty);
    ASSERT_TRUE(row.rootNonNull);
    EXPECT_TRUE(row.treeEmpty);
  }
}

TEST(TerminalRuleTest,
     WordLikeFreeFormTerminalCanStillInsertSynthesizedLeafAtEof) {
  TerminalRule<std::string_view> literal{"Literal", "abc"_kw};
  TerminalRule<std::string_view> terminal{"Token", literal};
  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(terminal, ctx));
  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(ctx.snapshotRecoveryEdits()));
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(ctx.cursorOffset(), 0u);
}

TEST(TerminalRuleTest,
     WordLikeFreeFormTerminalHasNoEntryProbeOnLocalVisibleSource) {
  TerminalRule<std::string_view> literal{"Literal", "abc"_kw};
  TerminalRule<std::string_view> terminal{"Token", literal};
  auto builderHarness = pegium::test::makeCstBuilderHarness("?");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  EXPECT_FALSE(terminal.probeRecoverableAtEntry(ctx));
}

TEST(TerminalRuleTest,
     CompositeWordLikeFreeFormTerminalCanStillInsertSynthesizedLeafAtEof) {
  TerminalRule<std::string_view> terminal{"Token", "a-zA-Z_"_cr + many(w)};
  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(terminal, ctx));
  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(ctx.snapshotRecoveryEdits()));
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Inserted);
  EXPECT_EQ(ctx.cursorOffset(), 0u);
}

TEST(TerminalRuleTest,
     DelimiterTerminalEntryProbeCanWakeAfterCompactSkippedTrivia) {
  TerminalRule<std::string_view> delimiter{"Delimiter", "("_kw};
  auto skipper = SkipperBuilder().ignore(some(s)).build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("   value");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  EXPECT_TRUE(delimiter.probeRecoverableAtEntry(ctx));
}

TEST(TerminalRuleTest, ZeroWidthRecoveredInsertSkipsConversionDiagnostic) {
  TerminalRule<int> number{"Number", "1"_kw,
                           opt::with_converter([](std::string_view) noexcept
                                                   -> opt::ConversionResult<int> {
                             return opt::conversion_error<int>(
                                 "bad recovered number");
                           })};
  auto builderHarness = pegium::test::makeCstBuilderHarness("");
  auto &builder = builderHarness.builder;
  auto skipper = SkipperBuilder().build();
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(number, ctx));

  const auto *root = builder.getRootCstNode();
  ASSERT_NE(root, nullptr);
  auto it = root->begin();
  ASSERT_NE(it, root->end());
  const auto node = *it;
  ASSERT_TRUE(node.isRecovered());
  ASSERT_EQ(node.getBegin(), node.getEnd());

  std::vector<ParseDiagnostic> diagnostics;
  const ValueBuildContext context{.diagnostics = &diagnostics};
  EXPECT_EQ(number.getRawValue(node, context), 0);
  EXPECT_TRUE(diagnostics.empty());
}

TEST(TerminalRuleTest, CanOverrideRule) {
  TerminalRule<bool> rule{"RuleWithOverrides", "a"_kw};
  auto skipper = SkipperBuilder().build();
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // first override
  rule = rule.super() | "b"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // second override
  rule = rule.super() | "c"_kw;
  EXPECT_TRUE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "d", skipper).fullMatch);

  // third override
  rule = "d"_kw;
  EXPECT_FALSE(parseTerminalRule(rule, "a", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "b", skipper).fullMatch);
  EXPECT_FALSE(parseTerminalRule(rule, "c", skipper).fullMatch);
  EXPECT_TRUE(parseTerminalRule(rule, "d", skipper).fullMatch);
}

TEST(TerminalRuleTest, EnumGetValueUsesUnderlyingVariantType) {
  TerminalRule<TerminalMode> mode{"Mode", "m"_kw};
  mode.setValueConverter([](std::string_view) noexcept {
    return opt::conversion_value<TerminalMode>(TerminalMode::Single);
  });

  auto parsed = parseTerminalRule(mode, "m", SkipperBuilder().build());
  ASSERT_TRUE(parsed.value);
  auto *typed =
      pegium::ast_ptr_cast<TerminalValueNode<TerminalMode>>(parsed.value);
  ASSERT_TRUE(typed != nullptr);
  EXPECT_EQ(typed->value, TerminalMode::Single);

  auto skipper = SkipperBuilder().build();
  auto builderHarness = pegium::test::makeCstBuilderHarness("m");
  auto &builder = builderHarness.builder;
  ParseContext ctx{builder, skipper};
  ASSERT_TRUE(parse(mode, ctx));
  auto root = builder.getRootCstNode();
  auto node = root->begin();
  ASSERT_NE(node, root->end());

  auto value = mode.getValue(*node);
  ASSERT_TRUE(std::holds_alternative<std::uint16_t>(value));
  EXPECT_EQ(std::get<std::uint16_t>(value), static_cast<std::uint16_t>(42));
}

TEST(TerminalRuleTest, DeleteStrayPrefixRecovery) {
  // Folds RecoveryPrefersDeletingStrayPrefixBeforeMatchingWord,
  // RecoveryPrefersDeletingShortStrayPunctuationRunBeforeMatchingDelimiter,
  // RecoveryProbeSeesExtendedDeleteScanMatchForLongPunctuationRun, and
  // RecoveryDeleteScanMatchesQuotedTerminalAfterDeletedPunctuationAndSkippedTrivia.
  // Every row asserts the same 9-step Deleted-recovery shape; only the
  // builder/input/skipper/endOffset/value vary. The value-node template type
  // differs per row (std::string vs std::string_view), so each row's `run`
  // builds its own terminal, casts to the matching node type, and returns the
  // already-extracted asserted values.
  struct DeletedRecovery {
    bool hasValue;
    bool fullMatch;
    std::size_t diagnosticCount;
    ParseDiagnosticKind kind;
    pegium::TextOffset beginOffset;
    pegium::TextOffset endOffset;
    bool typedNonNull;
    std::string value;
  };
  struct Case {
    const char *name;
    std::function<DeletedRecovery()> run;
  };
  static const Case kCases[] = {
      {"RecoveryPrefersDeletingStrayPrefixBeforeMatchingWord",
       [] {
         TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
         auto parsed = parseTerminalRule(id, ":qa", SkipperBuilder().build());
         DeletedRecovery row;
         row.hasValue = static_cast<bool>(parsed.value);
         row.fullMatch = parsed.fullMatch;
         row.diagnosticCount = parsed.parseDiagnostics.size();
         row.kind = parsed.parseDiagnostics.at(0).kind;
         row.beginOffset = parsed.parseDiagnostics.at(0).beginOffset;
         row.endOffset = parsed.parseDiagnostics.at(0).endOffset;
         auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string>>(
             parsed.value);
         row.typedNonNull = typed != nullptr;
         if (typed != nullptr) {
           row.value = std::string{typed->value};
         }
         return row;
       }},
      {"RecoveryPrefersDeletingShortStrayPunctuationRunBeforeMatchingDelimiter",
       [] {
         TerminalRule<std::string_view> semi{"Semi", ";"_kw};
         auto parsed =
             parseTerminalRule(semi, "***;", SkipperBuilder().build());
         DeletedRecovery row;
         row.hasValue = static_cast<bool>(parsed.value);
         row.fullMatch = parsed.fullMatch;
         row.diagnosticCount = parsed.parseDiagnostics.size();
         row.kind = parsed.parseDiagnostics.at(0).kind;
         row.beginOffset = parsed.parseDiagnostics.at(0).beginOffset;
         row.endOffset = parsed.parseDiagnostics.at(0).endOffset;
         auto *typed =
             pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
                 parsed.value);
         row.typedNonNull = typed != nullptr;
         if (typed != nullptr) {
           row.value = std::string{typed->value};
         }
         return row;
       }},
      {"RecoveryProbeSeesExtendedDeleteScanMatchForLongPunctuationRun",
       [] {
         TerminalRule<std::string_view> semi{"Semi", ";"_kw};
         const std::string text(9u, '*');
         auto parsed =
             parseTerminalRule(semi, text + ";", SkipperBuilder().build());
         DeletedRecovery row;
         row.hasValue = static_cast<bool>(parsed.value);
         row.fullMatch = parsed.fullMatch;
         row.diagnosticCount = parsed.parseDiagnostics.size();
         row.kind = parsed.parseDiagnostics.at(0).kind;
         row.beginOffset = parsed.parseDiagnostics.at(0).beginOffset;
         row.endOffset = parsed.parseDiagnostics.at(0).endOffset;
         // Original asserted endOffset == text.size() (9); the shared loop
         // covers this via kExpected[2].endOffset == 9u.
         auto *typed =
             pegium::ast_ptr_cast<TerminalValueNode<std::string_view>>(
                 parsed.value);
         row.typedNonNull = typed != nullptr;
         if (typed != nullptr) {
           row.value = std::string{typed->value};
         }
         return row;
       }},
      {"RecoveryDeleteScanMatchesQuotedTerminalAfterDeletedPunctuationAndSkippedTrivia",
       [] {
         const auto whitespace = some(s);
         const auto skipper = SkipperBuilder().ignore(whitespace).build();
         TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
         auto parsed = parseTerminalRule(text, ": \"team\"", skipper);
         DeletedRecovery row;
         row.hasValue = static_cast<bool>(parsed.value);
         row.fullMatch = parsed.fullMatch;
         row.diagnosticCount = parsed.parseDiagnostics.size();
         row.kind = parsed.parseDiagnostics.at(0).kind;
         row.beginOffset = parsed.parseDiagnostics.at(0).beginOffset;
         row.endOffset = parsed.parseDiagnostics.at(0).endOffset;
         auto *typed = pegium::ast_ptr_cast<TerminalValueNode<std::string>>(
             parsed.value);
         row.typedNonNull = typed != nullptr;
         if (typed != nullptr) {
           row.value = std::string{typed->value};
         }
         return row;
       }},
  };

  struct Expected {
    pegium::TextOffset endOffset;
    const char *value;
  };
  static const Expected kExpected[] = {
      {1u, "qa"},
      {3u, ";"},
      {9u, ";"},
      {1u, "\"team\""},
  };

  for (std::size_t i = 0; i < std::size(kCases); ++i) {
    SCOPED_TRACE(kCases[i].name);
    const auto row = kCases[i].run();
    ASSERT_TRUE(row.hasValue);
    EXPECT_TRUE(row.fullMatch);
    ASSERT_EQ(row.diagnosticCount, 1u);
    EXPECT_EQ(row.kind, ParseDiagnosticKind::Deleted);
    EXPECT_EQ(row.beginOffset, 0u);
    EXPECT_EQ(row.endOffset, kExpected[i].endOffset);
    ASSERT_TRUE(row.typedNonNull);
    EXPECT_EQ(row.value, kExpected[i].value);
  }
}

TEST(TerminalRuleTest,
     DirectRecoveryContextDeleteScanMatchesQuotedTerminalAfterSkippedTrivia) {
  const auto whitespace = some(s);
  const auto skipper = SkipperBuilder().ignore(whitespace).build();
  TerminalRule<std::string> text{"TEXT", "\""_kw <=> "\""_kw};
  auto builderHarness = pegium::test::makeCstBuilderHarness(": \"team\"");
  auto &builder = builderHarness.builder;
  detail::FailureHistoryRecorder recorder(builder.input_begin());

  RecoveryContext ctx{builder, skipper, recorder};
  ASSERT_TRUE(parse(text, ctx));

  const auto diagnostics = detail::materialize_syntax_diagnostics(
      detail::normalize_syntax_script(ctx.snapshotRecoveryEdits()));
  ASSERT_EQ(diagnostics.size(), 1u);
  EXPECT_EQ(diagnostics.front().kind, ParseDiagnosticKind::Deleted);
  EXPECT_EQ(diagnostics.front().beginOffset, 0u);
  EXPECT_EQ(diagnostics.front().endOffset, 1u);
  EXPECT_EQ(ctx.cursorOffset(),
            static_cast<pegium::TextOffset>(builder.getText().size()));
}
