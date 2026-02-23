#include <gtest/gtest.h>
#include <pegium/parser/Parser.hpp>

#include <algorithm>
#include <string>

using namespace pegium::parser;

TEST(RecoveryTest, DeleteBudgetIsConfigurable) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const std::string input = "xxxxxxxxxservice";
  const auto skipper = SkipperBuilder().build();

  const auto defaultResult = rule.parse(input, skipper);
  EXPECT_FALSE(defaultResult.ret);
  EXPECT_FALSE(defaultResult.recovered);

  ParseOptions options;
  options.maxConsecutiveCodepointDeletes = 16;
  const auto tunedResult = rule.parse(input, skipper, options);
  EXPECT_TRUE(tunedResult.ret);
  EXPECT_TRUE(tunedResult.recovered);
  EXPECT_EQ(tunedResult.len, input.size());
}

TEST(RecoveryTest, DiagnosticsTrackDeleteAndInsertEdits) {
  const auto skipper = SkipperBuilder().build();

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw};
    const std::string input = "oopsservice";
    const auto result = rule.parse(input, skipper);

    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.recovered);
    EXPECT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(std::all_of(result.diagnostics.begin(), result.diagnostics.end(),
                            [](const ParseDiagnostic &d) {
                              return d.kind == ParseDiagnosticKind::Deleted;
                            }));
  }

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw + "{"_kw + "}"_kw};
    const std::string input = "service{";
    const auto result = rule.parse(input, skipper);

    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.recovered);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(),
                            result.diagnostics.end(),
                            [](const ParseDiagnostic &d) {
                              return d.kind == ParseDiagnosticKind::Inserted;
                            }));
  }

  {
    DataTypeRule<std::string> rule{"Rule", "service"_kw};
    const std::string input = "servixe";
    const auto result = rule.parse(input, skipper);

    ASSERT_TRUE(result.ret);
    ASSERT_TRUE(result.recovered);
    EXPECT_TRUE(std::any_of(result.diagnostics.begin(),
                            result.diagnostics.end(),
                            [](const ParseDiagnostic &d) {
                              return d.kind == ParseDiagnosticKind::Replaced;
                            }));
  }
}

TEST(RecoveryTest, TypoRecoveryHandlesSubstitutionAndTransposition) {
  DataTypeRule<std::string> rule{"Rule", "service"_kw};
  const auto skipper = SkipperBuilder().build();

  {
    const std::string input = "servixe";
    const auto result = rule.parse(input, skipper);
    ASSERT_TRUE(result.ret);
    EXPECT_TRUE(result.recovered);
  }

  {
    const std::string input = "serivce";
    const auto result = rule.parse(input, skipper);
    ASSERT_TRUE(result.ret);
    EXPECT_TRUE(result.recovered);
  }

  {
    const std::string input = "sxrivxe";
    const auto result = rule.parse(input, skipper);
    EXPECT_FALSE(result.ret);
  }
}

TEST(RecoveryTest, AndPredicateRecoveryDoesNotUseEdits) {
  DataTypeRule<std::string> rule{"Rule", &"a"_kw + "a"_kw};
  const std::string input = "xa";
  const auto result = rule.parse(input, SkipperBuilder().build());

  EXPECT_FALSE(result.ret);
}
