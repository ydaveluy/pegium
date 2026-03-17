#include <gtest/gtest.h>

#include <sstream>

#include <pegium/services/JsonValue.hpp>

namespace pegium::services {
namespace {

TEST(JsonValueSerializationTest, SerializesCompactJsonDeterministically) {
  const JsonValue value{JsonValue::Object{
      {"z", 7},
      {"a", JsonValue::Array{true, "line\nbreak", nullptr}},
  }};

  EXPECT_EQ(value.toJsonString({.pretty = false}),
            R"({"a":[true,"line\nbreak",null],"z":7})");
}

TEST(JsonValueSerializationTest, SerializesPrettyJsonWithSortedObjectKeys) {
  const JsonValue value{JsonValue::Object{
      {"text", "demo"},
      {"object", JsonValue::Object{{"b", 2}, {"a", 1}}},
  }};

  EXPECT_EQ(value.toJsonString(),
            R"({
  "object": {
    "a": 1,
    "b": 2
  },
  "text": "demo"
})");
}

TEST(JsonValueSerializationTest, StreamsAsJson) {
  const JsonValue value{JsonValue::Object{{"a", 1}}};

  std::ostringstream stream;
  stream << value;

  EXPECT_EQ(stream.str(), R"({
  "a": 1
})");
}

} // namespace
} // namespace pegium::services
