// Exercises the public `pegium::testing::testCodeAction` helper against
// the arithmetics quick-fix that normalizes a
// constant expression — driven exactly the way a downstream language would.
#include <string>

#include <gtest/gtest.h>

#include <arithmetics/core/validation/ArithmeticsValidator.hpp>
#include <arithmetics/lsp/Module.hpp>

#include <pegium/testing/Testing.hpp>

namespace {

TEST(ArithmeticsCodeActionHarness, NormalizesConstantExpression) {
  pegium::testing::TestWorkspace ws;
  ws.registerLanguage(arithmetics::lsp::createArithmeticsServices(ws.shared()));

  const auto result = pegium::testing::testCodeAction(
      ws, "arithmetics",
      "module test\n"
      "def test: 2 + 3;\n",
      std::string(arithmetics::validation::IssueCodes::ExpressionNormalizable),
      "module test\n"
      "def test: 5;\n");

  ASSERT_TRUE(result.action.has_value());
}

} // namespace
