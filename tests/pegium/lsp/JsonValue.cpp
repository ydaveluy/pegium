#include <gtest/gtest.h>

#include <limits>

#include <lsp/types.h>

#include <pegium/lsp/JsonValue.hpp>

namespace pegium::lsp {
namespace {

TEST(JsonValueTest, RoundTripsNestedValuesBetweenPegiumAndLsp) {
  services::JsonValue::Object nestedObject;
  nestedObject.emplace("name", "demo");
  nestedObject.emplace("enabled", true);
  nestedObject.emplace("count", std::int64_t{42});

  services::JsonValue::Array nestedArray;
  nestedArray.emplace_back(nullptr);
  nestedArray.emplace_back(3.5);
  nestedArray.emplace_back(services::JsonValue::Object{
      {"flag", false},
  });

  services::JsonValue::Object root;
  root.emplace("object", std::move(nestedObject));
  root.emplace("array", std::move(nestedArray));

  const services::JsonValue value{std::move(root)};
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
      services::JsonValue{std::numeric_limits<std::int64_t>::max()});
  const auto smallest = to_lsp_any(
      services::JsonValue{std::numeric_limits<std::int64_t>::min()});

  ASSERT_TRUE(largest.isInteger());
  ASSERT_TRUE(smallest.isInteger());
  EXPECT_EQ(largest.integer(), std::numeric_limits<::lsp::json::Integer>::max());
  EXPECT_EQ(smallest.integer(), std::numeric_limits<::lsp::json::Integer>::min());
}

TEST(JsonValueTest, ConvertsDiagnosticSeverityBothWays) {
  constexpr std::array severities{
      services::DiagnosticSeverity::Error,
      services::DiagnosticSeverity::Warning,
      services::DiagnosticSeverity::Information,
      services::DiagnosticSeverity::Hint,
  };

  for (const auto severity : severities) {
    EXPECT_EQ(from_lsp_diagnostic_severity(to_lsp_diagnostic_severity(severity)),
              severity);
  }

  EXPECT_EQ(from_lsp_diagnostic_severity(::lsp::DiagnosticSeverity::MAX_VALUE),
            services::DiagnosticSeverity::Error);
}

} // namespace
} // namespace pegium::lsp
