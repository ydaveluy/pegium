#include <gtest/gtest.h>

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

TEST(JsonValueTest, PromotesOutOfRangeIntegersToDecimal) {
  // Within the 32-bit LSP wire range -> typed Integer, exact.
  const auto inRange = to_lsp_any(pegium::JsonValue{std::int64_t{123456}});
  ASSERT_TRUE(inRange.isInteger());
  EXPECT_EQ(inRange.integer(), 123456);

  // Outside int32 but exact as a double -> Decimal, value preserved (not
  // saturated to the int32 boundary).
  constexpr std::int64_t big = std::int64_t{1} << 40; // > int32, < 2^53
  const auto large = to_lsp_any(pegium::JsonValue{big});
  ASSERT_TRUE(large.isDecimal());
  EXPECT_EQ(large.decimal(), static_cast<::lsp::json::Decimal>(big));

  const auto small = to_lsp_any(pegium::JsonValue{-big});
  ASSERT_TRUE(small.isDecimal());
  EXPECT_EQ(small.decimal(), static_cast<::lsp::json::Decimal>(-big));
}

TEST(JsonValueTest, ConvertsDiagnosticSeverityToLsp) {
  EXPECT_EQ(to_lsp_diagnostic_severity(pegium::DiagnosticSeverity::Error),
            ::lsp::DiagnosticSeverity::Error);
  EXPECT_EQ(to_lsp_diagnostic_severity(pegium::DiagnosticSeverity::Warning),
            ::lsp::DiagnosticSeverity::Warning);
  EXPECT_EQ(to_lsp_diagnostic_severity(pegium::DiagnosticSeverity::Information),
            ::lsp::DiagnosticSeverity::Information);
  EXPECT_EQ(to_lsp_diagnostic_severity(pegium::DiagnosticSeverity::Hint),
            ::lsp::DiagnosticSeverity::Hint);
}

} // namespace
} // namespace pegium
