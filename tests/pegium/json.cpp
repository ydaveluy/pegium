#include <chrono>
#include <cstdint>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/parser/Parser.hpp>
#include <pegium/syntax-tree.hpp>
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

  std::variant<std::string, double, std::int32_t, JsonObject, JsonArray, bool,
               std::nullptr_t>
      value;
};

// @see https://www.json.org/json-en.html
class JsonParser : public Parser {
public:
  Terminal<> WS{"WS", some(s)};
  // "(\\.|[^"\\])*"
  Terminal<std::string> STRING{
      "STRING", "\""_kw + many("\\"_kw + dot | R"(^"\)"_cr) + "\""_kw};

  Terminal<double> Number{"Number",
                          option("-"_kw) + ("0"_kw | "1-9"_cr + many(d)) +
                              option("."_kw + some(d)) +
                              option("e"_cr.i() + option("-+"_cr) + some(d))};

  Terminal<bool> Bool{"Bool", "true"_kw | "false"_kw};
  Terminal<nullptr_t> Null{"Null", "null"_kw};

  /// STRING ':' value
  Rule<Json::Pair> Pair{"Pair", assign<&Pair::key>(STRING) + ":"_kw +
                                    assign<&Pair::value>(JsonValue)};

  /// '{' pair (',' pair)* '}' | '{' '}'
  Rule<Json::JsonObject> JsonObject{
      "JsonObject",
      "{"_kw + many(assign<&JsonObject::values>(Pair), ","_kw) + "}"_kw};

  /// '[' value (',' value)* ']' | '[' ']'
  Rule<Json::JsonArray> JsonArray{
      "JsonArray",
      "["_kw + many(assign<&JsonArray::values>(JsonValue), ","_kw) + "]"_kw};

  /// STRING | NUMBER | obj | arr | 'true' | 'false' | 'null'
  Rule<Json::JsonValue> JsonValue{
      "JsonValue",
      assign<&JsonValue::value>(STRING | JsonArray | Number | JsonObject |
                                JsonArray | Bool | Null)};

  JsonParser() {
    Null.setValueConverter([](std::string_view) { return nullptr; });
  }
  std::unique_ptr<IContext> createContext() const override {
    return ContextBuilder().ignore(WS).build();
  }
};

} // namespace Json

TEST(JsonTest, TestJson) {
  Json::JsonParser parser;

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

  for (std::size_t i = 0; i < 100'000; ++i) {
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

  std::cout << parser.STRING << ": " << *parser.STRING.getElement()
            << std::endl;
  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = parser.JsonValue.parse(input, parser.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "Parsed " << input.size() / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  EXPECT_TRUE(result.ret);
}
