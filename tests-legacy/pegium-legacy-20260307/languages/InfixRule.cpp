#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace pegium::parser;

namespace {

struct Expr : pegium::AstNode {};

struct NameExpr : Expr {
  string name;
};

struct BinaryExpr : Expr {
  pointer<Expr> left;
  string op;
  pointer<Expr> right;
};

class InfixProbeParser final : public PegiumParser {
public:
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<> ID{"ID", "a-z"_cr + many(w)};

  Rule<Expr> Primary{
      "Primary", create<NameExpr>() + assign<&NameExpr::name>(ID)};

  InfixRule<BinaryExpr, &BinaryExpr::left, &BinaryExpr::op, &BinaryExpr::right>
      Binary{"Binary",
             Primary,
             LeftAssociation("%"_kw),
             LeftAssociation("^"_kw),
             LeftAssociation("*"_kw | "/"_kw),
             LeftAssociation("+"_kw | "-"_kw)};

  Rule<Expr> Root{"Root", Binary};
#pragma clang diagnostic pop
};

template <typename ParserType>
std::unique_ptr<pegium::workspace::Document>
parse_text(const ParserType &parser, std::string_view text) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  parser.parse(*document);
  return document;
}

std::string makeInfixProbePayload(std::size_t operatorCount) {
  std::string payload;
  payload.reserve(operatorCount * 2 + 1);
  payload.push_back('a');
  for (std::size_t index = 0; index < operatorCount; ++index) {
    payload.push_back(index % 2 == 0 ? '-' : '+');
    payload.push_back('a');
  }
  return payload;
}

constexpr std::string_view kInfixOperatorProbeBenchmarkName =
    "infix-operator-probe";

} // namespace

TEST(InfixRuleBenchmark, HasOperatorProbeMicroBenchmark) {
  InfixProbeParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_INFIX_REPETITIONS", 12'000, /*minValue*/ 1);
  const auto payload =
      makeInfixProbePayload(static_cast<std::size_t>(repetitions));

  const auto probe = parse_text(parser, payload);
  ASSERT_TRUE(probe->parseResult.value);
  ASSERT_TRUE(probe->parseResult.fullMatch);
  ASSERT_TRUE(probe->parseResult.parseDiagnostics.empty());

  const auto stats = pegium::test::runParseBenchmark(
      kInfixOperatorProbeBenchmarkName, payload,
      [&](std::string_view text) { return parse_text(parser, text); });
  pegium::test::assertMinThroughput("infix_operator_probe", stats.mib_per_s);
}
