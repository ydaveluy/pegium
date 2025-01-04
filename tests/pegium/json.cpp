#include <charconv>
#include <chrono>
#include <gtest/gtest.h>
#include <iostream>
#include <pegium/parser/Parser.hpp>
#include <string>
#include <string_view>
#include <variant>

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

  std::variant<std::string, double, pointer<JsonObject>, pointer<JsonArray>,
               bool, std::monostate>
      value;
};

class Parser : public pegium::parser::Parser {
public:
#include <pegium/rule_macros_begin.h>
  TERM(WS);
  TERM(STRING);
  TERM(Number, double);
  TERM(TRUE, bool);
  TERM(FALSE, bool);
  TERM(NULL_KW, std::monostate);

  RULE(Pair, Json::Pair);
  RULE(JsonValue, Json::JsonValue);
  RULE(JsonArray, Json::JsonArray);
  RULE(JsonObject, Json::JsonObject);

#include <pegium/rule_macros_end.h>

  Parser() {

    using namespace pegium::grammar;
    WS = at_least_one(s);

    STRING = "\""_kw + many(!"\""_cr) + "\""_kw;

    Number = opt("-"_kw) + ("0"_kw | "1-9"_cr + many("0-9"_cr)) +
             opt("."_kw + at_least_one("0-9"_cr)) +
             opt("e"_kw.i() + opt("-+"_cr) + at_least_one("0-9"_cr));

    TRUE = "true"_kw;
    TRUE.setValueConverter([](std::string_view) { return true; });
    FALSE = "false"_kw;
    FALSE.setValueConverter([](std::string_view) { return false; });
    NULL_KW = "null"_kw;
    NULL_KW.setValueConverter(
        [](std::string_view) { return std::monostate{}; });

    /// STRING ':' value
    Pair =
        assign<&Pair::key>(STRING) + ":"_kw + assign<&Pair::value>(JsonValue);

    /// '{' pair (',' pair)* '}' | '{' '}'
    JsonObject =
        "{"_kw + many_sep(assign<&JsonObject::values>(Pair), ","_kw) + "}"_kw;

    /// '[' value (',' value)* ']' | '[' ']'
    JsonArray = "["_kw +
                many_sep(assign<&JsonArray::values>(JsonValue), ","_kw) +
                "]"_kw;

    /// STRING | NUMBER | obj | arr | 'true' | 'false' | 'null'
    JsonValue = assign<&JsonValue::value>(STRING | JsonArray /*| Number | JsonObject |
                                          JsonArray | TRUE | FALSE | NULL_KW*/);
  }
  std::unique_ptr<pegium::grammar::IContext> createContext() const override {
    return ContextBuilder().ignore(WS).build();
  }
};

} // namespace Json

TEST(JsonTest, TestJson) {
  Json::Parser g;

  std::string input = R"(
{ "type": "FeatureCollection",
  "features": [
{
    "type": "Feature",
"properties": { "name": "Canada" }
}
]
}

  )";

  using namespace std::chrono;
  auto start = high_resolution_clock::now();

  auto result = g.JsonValue.parse(input, g.createContext());
  auto end = high_resolution_clock::now();
  auto duration = duration_cast<milliseconds>(end - start).count();

  std::cout << "Parsed " << input.size() / static_cast<double>(1024 * 1024)
            << " Mo in " << duration << "ms: "
            << ((1000. * result.len / duration) /
                static_cast<double>(1024 * 1024))
            << " Mo/s\n";

  EXPECT_TRUE(result.ret);
}
