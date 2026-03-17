#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/parser/ParseContext.hpp>
#include <pegium/syntax-tree/CstBuilder.hpp>
#include <pegium/syntax-tree/RootCstNode.hpp>
#include <pegium/workspace/Document.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace pegium::parser;

namespace {

const TerminalRule<> kLeafElement{"Leaf", "a"_kw};
const TerminalRule<> kGroupElement{"Group", "a"_kw};
constexpr std::string_view kParseContextStrictBenchmarkName =
    "parse-context-strict-leaf-exit";

std::string make_parse_context_payload(std::size_t leafCount) {
  return std::string(leafCount, 'a');
}

} // namespace

TEST(ParseContextStrictBenchmark, LeafExitMicroBenchmark) {
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_PARSE_CONTEXT_REPETITIONS", 80'000, /*minValue*/ 1);
  const auto payload =
      make_parse_context_payload(static_cast<std::size_t>(repetitions));
  const auto skipper = skip();

  const auto stats = pegium::test::runWorkBenchmark(
      kParseContextStrictBenchmarkName, payload.size(), [&]() {
        pegium::workspace::Document document;
        document.setText(payload);
        pegium::RootCstNode root{document};
        pegium::CstBuilder builder{root};
        ParseContext ctx{builder, skipper};

        while (ctx.cursor() != ctx.end) {
          const auto *nodeStart = ctx.enter();
          ctx.leaf(nodeStart + 1, std::addressof(kLeafElement));
          ctx.exit(nodeStart, std::addressof(kGroupElement));
        }

        EXPECT_EQ(ctx.cursor(), ctx.end);
        EXPECT_EQ(builder.node_count(),
                  static_cast<pegium::NodeCount>(payload.size() * 2));
      });
  pegium::test::assertMinThroughput("parse_context_strict", stats.mib_per_s);
}
