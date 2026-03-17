#include <gtest/gtest.h>
#include <pegium/benchmarks.hpp>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/workspace/Document.hpp>

#include <memory>
#include <string>
#include <string_view>

using namespace pegium::parser;

namespace {

struct NameList : pegium::AstNode {
  vector<string> names;
};

class RepetitionBenchmarkParser final : public PegiumParser {
public:
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Root;
  }

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wuninitialized"
  Terminal<> ID{"ID", "a-z"_cr + many(w)};

  Rule<NameList> Root{
      "Root", create<NameList>() + some(append<&NameList::names>(ID), ","_kw)};
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

std::string make_repetition_payload(std::size_t itemCount) {
  std::string payload;
  payload.reserve(itemCount * 2);
  payload.push_back('a');
  for (std::size_t index = 1; index < itemCount; ++index) {
    payload.push_back(',');
    payload.push_back(static_cast<char>('a' + (index % 3)));
  }
  return payload;
}

constexpr std::string_view kRepetitionBenchmarkName =
    "repetition-separated-identifiers";

} // namespace

TEST(RepetitionBenchmark, ParseSpeedMicroBenchmark) {
  RepetitionBenchmarkParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_REPETITION_REPETITIONS", 20'000, /*minValue*/ 1);
  const auto payload =
      make_repetition_payload(static_cast<std::size_t>(repetitions));

  const auto probe = parse_text(parser, payload);
  ASSERT_TRUE(probe->parseResult.value);
  ASSERT_TRUE(probe->parseResult.fullMatch);
  ASSERT_TRUE(probe->parseResult.parseDiagnostics.empty());

  const auto stats = pegium::test::runParseBenchmark(
      kRepetitionBenchmarkName, payload,
      [&](std::string_view text) { return parse_text(parser, text); });
  pegium::test::assertMinThroughput("repetition_separated_identifiers",
                                    stats.mib_per_s);
}
