#include <gtest/gtest.h>

#include <limits>
#include <sstream>
#include <stdexcept>

#include <pegium/core/services/JsonValue.hpp>

namespace pegium {
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

TEST(JsonValueSerializationTest, NonFiniteDoublesBecomeNull) {
  const JsonValue nan{std::numeric_limits<double>::quiet_NaN()};
  const JsonValue infinity{std::numeric_limits<double>::infinity()};
  const JsonValue negativeInfinity{-std::numeric_limits<double>::infinity()};

  EXPECT_TRUE(nan.isNull());
  EXPECT_TRUE(infinity.isNull());
  EXPECT_TRUE(negativeInfinity.isNull());
  EXPECT_EQ(infinity.toJsonString({.pretty = false}), "null");
}

TEST(JsonValueSerializationTest, IntegerRejectsOutOfRangeDoubles) {
  EXPECT_THROW((void)JsonValue(std::numeric_limits<double>::max()).integer(),
               std::out_of_range);
  EXPECT_THROW((void)JsonValue(-std::numeric_limits<double>::max()).integer(),
               std::out_of_range);
  EXPECT_EQ(JsonValue(42.75).integer(), 42);
}

} // namespace
} // namespace pegium
