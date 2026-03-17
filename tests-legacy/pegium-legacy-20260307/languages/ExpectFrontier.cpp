#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>

#include <string>
#include <string_view>

using namespace pegium::parser;

namespace {

struct ExpectFrontierNode : pegium::AstNode {};

class ExpectFrontierBenchmarkParser final : public PegiumParser {
public:
  using PegiumParser::expect;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

  const Skipper &getSkipper() const noexcept override { return skipper; }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  static constexpr auto WS = some(s);
  Skipper skipper = skip(ignored(WS));
  Terminal<> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<ExpectFrontierNode> Root{
      "Root",
      create<ExpectFrontierNode>() + many("entry"_kw + ID + ";"_kw) +
          "start"_kw + ("left"_kw | "right"_kw | "end"_kw)};
#pragma clang diagnostic pop
};

std::string make_expect_frontier_payload(std::size_t prefixCount) {
  std::string payload;
  payload.reserve(prefixCount * 10 + 6);
  for (std::size_t index = 0; index < prefixCount; ++index) {
    payload += "entry ";
    payload.push_back(static_cast<char>('a' + (index % 3)));
    payload += "; ";
  }
  payload += "start";
  return payload;
}

constexpr std::string_view kExpectFrontierBenchmarkName =
    "expect-frontier-start-choice";

} // namespace

TEST(ExpectFrontierBenchmark, ExpectSpeedMicroBenchmark) {
  ExpectFrontierBenchmarkParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_EXPECT_FRONTIER_REPETITIONS", 12'000, /*minValue*/ 1);
  const auto payload =
      make_expect_frontier_payload(static_cast<std::size_t>(repetitions));
  const auto offset = static_cast<pegium::TextOffset>(payload.size());

  const auto probe = parser.expect(payload, offset);
  ASSERT_TRUE(probe.reachedAnchor);
  ASSERT_FALSE(probe.frontier.empty());

  const auto stats = pegium::test::runExpectBenchmark(
      kExpectFrontierBenchmarkName, payload, payload.size(),
      [&](std::string_view text, std::size_t anchorOffset) {
        return parser.expect(text, static_cast<pegium::TextOffset>(anchorOffset));
      });
  pegium::test::assertMinThroughput("expect_frontier", stats.mib_per_s);
}
