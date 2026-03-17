#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/parser/PegiumParser.hpp>
#include <pegium/benchmarks.hpp>
#include <pegium/syntax-tree/AstNode.hpp>
#include <pegium/workspace/Document.hpp>
#include <string>
#include <string_view>
#include <variant>

using namespace pegium::parser;

namespace Json {

struct JsonValue;

struct Pair : pegium::AstNode {
  string key;
  pointer<JsonValue> value;
};

struct JsonObject : pegium::AstNode {
  vector<pointer<Pair>> values;
};

struct JsonArray : pegium::AstNode {
  vector<pointer<JsonValue>> values;
};

struct JsonValue : pegium::AstNode {

  std::variant<std::string, double, std::int32_t, pointer<JsonObject>,
               pointer<JsonArray>, bool, std::nullptr_t>
      value;
};

// @see https://www.json.org/json-en.html
class JsonParser : public PegiumParser {
public:
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return JsonValue;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

  static constexpr auto WS = some(s);
  Skipper skipper = SkipperBuilder().ignore(WS).build();
  // "(\\.|[^"\\])*"
  Terminal<std::string> STRING{
      "STRING", "\""_kw + many("\\"_kw + dot | R"(^"\)"_cr) + "\""_kw};

  Terminal<double> Number{"Number",
                          option("-"_kw) + ("0"_kw | "1-9"_cr + many(d)) +
                              option("."_kw + some(d)) +
                              option("eE"_cr + option("-+"_cr) + some(d))};

  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Terminal<std::nullptr_t> Null{"Null", "null"_kw};

  /// STRING ':' value
  Rule<Json::Pair> Pair{"Pair", assign<&Pair::key>(STRING) + ":"_kw +
                                    assign<&Pair::value>(JsonValue)};

  /// '{' pair (',' pair)* '}' | '{' '}'
  Rule<Json::JsonObject> JsonObject{
      "JsonObject",
      "{"_kw + many(append<&JsonObject::values>(Pair), ","_kw) + "}"_kw};

  /// '[' value (',' value)* ']' | '[' ']'
  Rule<Json::JsonArray> JsonArray{
      "JsonArray",
      "["_kw + many(append<&JsonArray::values>(JsonValue), ","_kw) + "]"_kw};

  /// STRING | NUMBER | obj | arr | 'true' | 'false' | 'null'
  Rule<Json::JsonValue> JsonValue{
      "JsonValue", assign<&JsonValue::value>(STRING | Number | JsonObject |
                                             JsonArray | Bool | Null)};

public:
  JsonParser() {
    Null.setValueConverter([](std::string_view) noexcept {
      return opt::conversion_value<std::nullptr_t>(nullptr);
    });
  }
};

} // namespace Json

namespace {

std::string makeJsonPayload(std::size_t repetitions) {
  std::string input = R"(
{ 
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": { "name": "Canada" }
    }
  ],


  )";
  for (std::size_t i = 0; i < repetitions; ++i) {
    input += R"(    
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": { "name": "Canada" },
      "number": -1.5e3 ,
      "true": true,
      "false": false,
      "null": null
    }
  ],
    )";
  }
  input += R"(
  "type": "FeatureCollection",
  "features": [
    {
      "type": "Feature",
      "properties": { "name": "Canada" }
    }
  ]
})";
  return input;
}

template <typename ParserType>
auto parse_text(const ParserType &parser, std::string_view text,
                       const ParseOptions &options = {}) {
  (void)options;
  auto document = std::make_unique<pegium::workspace::Document>();
  document->setText(std::string{text});
  parser.parse(*document);
  return std::move(document);
}

} // namespace


TEST(JsonBenchmark, ParseSpeedMicroBenchmark) {
  Json::JsonParser parser;
  const auto repetitions = pegium::test::getEnvInt(
      "PEGIUM_BENCH_JSON_REPETITIONS", 5'000, /*minValue*/ 1);
  const auto payload = makeJsonPayload(static_cast<std::size_t>(repetitions));

  const auto stats = pegium::test::runParseBenchmark(
      "json", payload, [&](std::string_view text) {
        return parse_text(parser, text);
      });
  pegium::test::assertMinThroughput("json", stats.mib_per_s);
}
