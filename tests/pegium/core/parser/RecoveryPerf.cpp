#include <gtest/gtest.h>

#include <chrono>
#include <sstream>
#include <string>

#include <pegium/core/ParseJsonTestSupport.hpp>
#include <pegium/core/parser/PegiumParser.hpp>

using namespace pegium::parser;

namespace {

struct RecoveryPerfStatementNode : pegium::AstNode {
  std::string name;
};

struct RecoveryPerfModuleNode : pegium::AstNode {
  std::string name;
  std::vector<std::unique_ptr<RecoveryPerfStatementNode>> statements;
};

struct RecoveryPerfFixture : public ::testing::Test {
  TerminalRule<> ws{"WS", some(s)};
  TerminalRule<> slComment{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  TerminalRule<std::string> id{"ID", "a-zA-Z_"_cr + many(w)};
  Skipper skipper =
      SkipperBuilder().ignore(ws).hide(slComment).build();

  ParserRule<RecoveryPerfStatementNode> statement{
      "Statement", assign<&RecoveryPerfStatementNode::name>(id) + ";"_kw};
  ParserRule<RecoveryPerfModuleNode> module{
      "Module", "module"_kw + assign<&RecoveryPerfModuleNode::name>(id) +
                    many(append<&RecoveryPerfModuleNode::statements>(
                        statement))};
};

std::string make_missing_semicolon_module(std::size_t statementCount,
                                          std::size_t missingEvery) {
  std::ostringstream out;
  out << "module m\n";
  for (std::size_t i = 0; i < statementCount; ++i) {
    out << "stmt" << i;
    if (missingEvery == 0 || (i % missingEvery) != 0) {
      out << ";";
    }
    out << " // " << i << "\n";
  }
  return std::move(out).str();
}

} // namespace

TEST_F(RecoveryPerfFixture,
       MultiSiteRecoveryAttemptRunsScaleLinearlyAcrossMissingSemicolons) {
  const auto text = make_missing_semicolon_module(20, 4);
  const auto start = std::chrono::steady_clock::now();
  const auto result = pegium::test::Parse(module, text, skipper, {});
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(result.fullMatch);
  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_LT(result.recoveryReport.recoveryAttemptRuns, 512u);
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count(),
            200);
}

TEST_F(RecoveryPerfFixture,
       DenseMissingSemicolonRecoveryStaysUnderGlobalAttemptBudget) {
  const auto text = make_missing_semicolon_module(16, 1);
  const auto start = std::chrono::steady_clock::now();
  const auto result = pegium::test::Parse(module, text, skipper, {});
  const auto elapsed = std::chrono::steady_clock::now() - start;

  EXPECT_TRUE(result.recoveryReport.hasRecovered);
  EXPECT_LT(result.recoveryReport.recoveryAttemptRuns, 1024u);
  EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed)
                .count(),
            250);
}

TEST_F(RecoveryPerfFixture, WellFormedInputPaysNoRecoveryCost) {
  const auto text = make_missing_semicolon_module(40, 0);
  const auto result = pegium::test::Parse(module, text, skipper, {});

  EXPECT_TRUE(result.fullMatch);
  EXPECT_FALSE(result.recoveryReport.hasRecovered);
  EXPECT_EQ(result.recoveryReport.recoveryAttemptRuns, 0u);
}
