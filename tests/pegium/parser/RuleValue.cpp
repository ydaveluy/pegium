#include <gtest/gtest.h>

#include <pegium/parser/RuleValue.hpp>

#include <string>
#include <string_view>
#include <variant>
#include <vector>

using pegium::parser::detail::SupportedRuleValueType;
using pegium::parser::detail::toRuleValue;

static_assert(SupportedRuleValueType<std::string_view>);
static_assert(SupportedRuleValueType<std::string>);
static_assert(SupportedRuleValueType<char>);
static_assert(SupportedRuleValueType<bool>);
static_assert(SupportedRuleValueType<std::int8_t>);
static_assert(SupportedRuleValueType<std::uint64_t>);
static_assert(SupportedRuleValueType<double>);
static_assert(!SupportedRuleValueType<std::vector<int>>);

TEST(RuleValueTest, ConvertsStringAndSpecialTypes) {
  {
    auto value = toRuleValue(std::string_view{"abc"});
    ASSERT_TRUE(std::holds_alternative<std::string_view>(value));
    EXPECT_EQ(std::get<std::string_view>(value), "abc");
  }

  {
    auto value = toRuleValue(std::string{"xyz"});
    ASSERT_TRUE(std::holds_alternative<std::string>(value));
    EXPECT_EQ(std::get<std::string>(value), "xyz");
  }

  {
    auto value = toRuleValue('q');
    ASSERT_TRUE(std::holds_alternative<char>(value));
    EXPECT_EQ(std::get<char>(value), 'q');
  }

  {
    auto value = toRuleValue(true);
    ASSERT_TRUE(std::holds_alternative<bool>(value));
    EXPECT_TRUE(std::get<bool>(value));
  }

  {
    auto value = toRuleValue(nullptr);
    ASSERT_TRUE(std::holds_alternative<std::nullptr_t>(value));
    EXPECT_EQ(std::get<std::nullptr_t>(value), nullptr);
  }
}

TEST(RuleValueTest, ConvertsSignedIntegralsByWidth) {
  {
    auto value = toRuleValue(static_cast<signed char>(-5));
    ASSERT_TRUE(std::holds_alternative<std::int8_t>(value));
    EXPECT_EQ(std::get<std::int8_t>(value), static_cast<std::int8_t>(-5));
  }

  {
    auto value = toRuleValue(static_cast<short>(-300));
    ASSERT_TRUE(std::holds_alternative<std::int16_t>(value));
    EXPECT_EQ(std::get<std::int16_t>(value), static_cast<std::int16_t>(-300));
  }

  {
    auto value = toRuleValue(static_cast<int>(-70000));
    ASSERT_TRUE(std::holds_alternative<std::int32_t>(value));
    EXPECT_EQ(std::get<std::int32_t>(value), static_cast<std::int32_t>(-70000));
  }

  {
    auto value = toRuleValue(static_cast<long long>(-9000000000LL));
    ASSERT_TRUE(std::holds_alternative<std::int64_t>(value));
    EXPECT_EQ(std::get<std::int64_t>(value),
              static_cast<std::int64_t>(-9000000000LL));
  }
}

TEST(RuleValueTest, ConvertsUnsignedIntegralsByWidth) {
  {
    auto value = toRuleValue(static_cast<unsigned char>(250));
    ASSERT_TRUE(std::holds_alternative<std::uint8_t>(value));
    EXPECT_EQ(std::get<std::uint8_t>(value), static_cast<std::uint8_t>(250));
  }

  {
    auto value = toRuleValue(static_cast<unsigned short>(60000));
    ASSERT_TRUE(std::holds_alternative<std::uint16_t>(value));
    EXPECT_EQ(std::get<std::uint16_t>(value),
              static_cast<std::uint16_t>(60000));
  }

  {
    auto value = toRuleValue(static_cast<unsigned int>(4000000000U));
    ASSERT_TRUE(std::holds_alternative<std::uint32_t>(value));
    EXPECT_EQ(std::get<std::uint32_t>(value),
              static_cast<std::uint32_t>(4000000000U));
  }

  {
    auto value = toRuleValue(static_cast<unsigned long long>(9000000000ULL));
    ASSERT_TRUE(std::holds_alternative<std::uint64_t>(value));
    EXPECT_EQ(std::get<std::uint64_t>(value),
              static_cast<std::uint64_t>(9000000000ULL));
  }
}

TEST(RuleValueTest, ConvertsFloatingPointTypes) {
  {
    auto value = toRuleValue(1.5f);
    ASSERT_TRUE(std::holds_alternative<float>(value));
    EXPECT_FLOAT_EQ(std::get<float>(value), 1.5f);
  }

  {
    auto value = toRuleValue(2.25);
    ASSERT_TRUE(std::holds_alternative<double>(value));
    EXPECT_DOUBLE_EQ(std::get<double>(value), 2.25);
  }

  {
    auto value = toRuleValue(static_cast<long double>(3.75L));
    ASSERT_TRUE(std::holds_alternative<long double>(value));
    EXPECT_EQ(std::get<long double>(value), static_cast<long double>(3.75L));
  }
}
