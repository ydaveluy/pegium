#include <gtest/gtest.h>

#include <limits>

#include <lsp/types.h>

#include <pegium/lsp/support/JsonValue.hpp>

namespace pegium {
namespace {

TEST(JsonValueTest, RoundTripsNestedValuesBetweenPegiumAndLsp) {
  pegium::JsonValue::Object nestedObject;
  nestedObject.try_emplace("name", "demo");
  nestedObject.try_emplace("enabled", true);
  nestedObject.try_emplace("count", std::int64_t{42});

  pegium::JsonValue::Array nestedArray;
  nestedArray.emplace_back(nullptr);
  nestedArray.emplace_back(3.5);
  nestedArray.emplace_back(pegium::JsonValue::Object{
      {"flag", false},
  });

  pegium::JsonValue::Object root;
  root.try_emplace("object", std::move(nestedObject));
  root.try_emplace("array", std::move(nestedArray));

  const pegium::JsonValue value{std::move(root)};
  const auto roundTripped = from_lsp_any(to_lsp_any(value));

  ASSERT_TRUE(roundTripped.isObject());
  const auto &rootObject = roundTripped.object();
  ASSERT_TRUE(rootObject.contains("object"));
  ASSERT_TRUE(rootObject.contains("array"));

  const auto &object = rootObject.at("object");
  ASSERT_TRUE(object.isObject());
  EXPECT_EQ(object.object().at("name").string(), "demo");
  EXPECT_TRUE(object.object().at("enabled").boolean());
  EXPECT_EQ(object.object().at("count").integer(), 42);

  const auto &array = rootObject.at("array");
  ASSERT_TRUE(array.isArray());
  ASSERT_EQ(array.array().size(), 3u);
  EXPECT_TRUE(array.array()[0].isNull());
  EXPECT_DOUBLE_EQ(array.array()[1].number(), 3.5);
  ASSERT_TRUE(array.array()[2].isObject());
  EXPECT_FALSE(array.array()[2].object().at("flag").boolean());
}

TEST(JsonValueTest, ClampsIntegersToLspIntegerRange) {
  const auto largest = to_lsp_any(
      pegium::JsonValue{std::numeric_limits<std::int64_t>::max()});
  const auto smallest = to_lsp_any(
      pegium::JsonValue{std::numeric_limits<std::int64_t>::min()});

  ASSERT_TRUE(largest.isInteger());
  ASSERT_TRUE(smallest.isInteger());
  EXPECT_EQ(largest.integer(), std::numeric_limits<::lsp::json::Integer>::max());
  EXPECT_EQ(smallest.integer(), std::numeric_limits<::lsp::json::Integer>::min());
}

TEST(JsonValueTest, ConvertsDiagnosticSeverityBothWays) {
  constexpr std::array severities{
      pegium::DiagnosticSeverity::Error,
      pegium::DiagnosticSeverity::Warning,
      pegium::DiagnosticSeverity::Information,
      pegium::DiagnosticSeverity::Hint,
  };

  for (const auto severity : severities) {
    EXPECT_EQ(from_lsp_diagnostic_severity(to_lsp_diagnostic_severity(severity)),
              severity);
  }

  EXPECT_EQ(from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity::MAX_VALUE),
            pegium::DiagnosticSeverity::Error);
}

} // namespace
} // namespace pegium
