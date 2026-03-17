#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/parser/ValueBuildContext.hpp>
#include <pegium/workspace/Document.hpp>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

using namespace pegium::parser;

namespace {

struct ValueBuildItem : pegium::AstNode {
  string name;
  string value;
};

struct ValueBuildRoot : pegium::AstNode {
  vector<pointer<ValueBuildItem>> items;
};

class ValueBuildBenchmarkParser final : public PegiumParser {
public:
  using PegiumParser::parse;

  const pegium::grammar::ParserRule &entryRuleForBenchmark() const noexcept {
    return Root;
  }

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<std::string_view> ID{"ID", "a-z"_cr + many(w)};

  Rule<ValueBuildItem> Item{
      "Item", create<ValueBuildItem>() + assign<&ValueBuildItem::name>(ID) +
                  "="_kw + assign<&ValueBuildItem::value>(ID) + ";"_kw};

  Rule<ValueBuildRoot> Root{
      "Root", create<ValueBuildRoot>() + some(append<&ValueBuildRoot::items>(Item))};
#pragma clang diagnostic pop
};

template <typename ParserType>
std::unique_ptr<pegium::workspace::Document> parse_text(const ParserType &parser,
                                                        std::string_view text) {
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  parser.parse(*document);
  return document;
}

std::string make_value_build_payload(std::size_t itemCount) {
  std::string payload;
  payload.reserve(itemCount * 12);
  for (std::size_t index = 0; index < itemCount; ++index) {
    payload += "item";
    payload.push_back(static_cast<char>('a' + (index % 5)));
    payload.push_back('=');
    payload += "value";
    payload.push_back(static_cast<char>('a' + (index % 7)));
    payload.push_back(';');
  }
  return payload;
}

constexpr std::string_view kValueBuildBenchmarkName = "value-build-parser-rule";

} // namespace

TEST(ValueBuildBenchmark, ParserRuleBuildMicroBenchmark) {
  ValueBuildBenchmarkParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_VALUE_BUILD_REPETITIONS", 12'000, /*minValue*/ 1);
  const auto payload =
      make_value_build_payload(static_cast<std::size_t>(repetitions));

  auto document = parse_text(parser, payload);
  ASSERT_TRUE(document->parseResult.fullMatch);
  ASSERT_TRUE(document->parseResult.cst != nullptr);
  auto it = document->parseResult.cst->begin();
  ASSERT_NE(it, document->parseResult.cst->end());
  const auto entryNode = *it;

  const auto &entryRule = parser.entryRuleForBenchmark();
  const auto stats = pegium::test::runWorkBenchmark(
      kValueBuildBenchmarkName, payload.size(), [&]() {
        pegium::parser::ValueBuildContext context;
        auto value = entryRule.getValue(entryNode, context);
        ASSERT_TRUE(value != nullptr);
      });
  pegium::test::assertMinThroughput("value_build_parser_rule", stats.mib_per_s);
}
