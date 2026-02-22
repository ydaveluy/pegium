#pragma once

#include <cstdint>
#include <pegium/grammar/RuleValue.hpp>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <variant>

namespace pegium::parser::detail {

using RuleValue = grammar::RuleValue;

template <typename T>
concept SupportedRuleValueType =
    std::same_as<std::remove_cvref_t<T>, std::string_view> ||
    std::same_as<std::remove_cvref_t<T>, std::string> ||
    std::same_as<std::remove_cvref_t<T>, char> ||
    std::same_as<std::remove_cvref_t<T>, bool> ||
    std::same_as<std::remove_cvref_t<T>, std::nullptr_t> ||
    (std::integral<std::remove_cvref_t<T>> &&
     !std::same_as<std::remove_cvref_t<T>, char> &&
     !std::same_as<std::remove_cvref_t<T>, bool>) ||
    std::floating_point<std::remove_cvref_t<T>>;

template <typename T>
  requires SupportedRuleValueType<T>
inline RuleValue toRuleValue(T value) {
  using RawT = std::remove_cvref_t<T>;

  if constexpr (std::same_as<RawT, std::string_view>) {
    return RuleValue{value};
  } else if constexpr (std::same_as<RawT, std::string>) {
    return RuleValue{std::move(value)};
  } else if constexpr (std::same_as<RawT, char>) {
    return RuleValue{value};
  } else if constexpr (std::same_as<RawT, bool>) {
    return RuleValue{value};
  } else if constexpr (std::same_as<RawT, std::nullptr_t>) {
    return RuleValue{nullptr};
  } else if constexpr (std::floating_point<RawT>) {
    if constexpr (std::same_as<RawT, float>) {
      return RuleValue{value};
    } else if constexpr (std::same_as<RawT, double>) {
      return RuleValue{value};
    } else {
      return RuleValue{static_cast<long double>(value)};
    }
  } else if constexpr (std::integral<RawT>) {
    if constexpr (std::is_signed_v<RawT>) {
      if constexpr (sizeof(RawT) <= sizeof(std::int8_t)) {
        return RuleValue{static_cast<std::int8_t>(value)};
      } else if constexpr (sizeof(RawT) <= sizeof(std::int16_t)) {
        return RuleValue{static_cast<std::int16_t>(value)};
      } else if constexpr (sizeof(RawT) <= sizeof(std::int32_t)) {
        return RuleValue{static_cast<std::int32_t>(value)};
      } else {
        return RuleValue{static_cast<std::int64_t>(value)};
      }
    } else {
      if constexpr (sizeof(RawT) <= sizeof(std::uint8_t)) {
        return RuleValue{static_cast<std::uint8_t>(value)};
      } else if constexpr (sizeof(RawT) <= sizeof(std::uint16_t)) {
        return RuleValue{static_cast<std::uint16_t>(value)};
      } else if constexpr (sizeof(RawT) <= sizeof(std::uint32_t)) {
        return RuleValue{static_cast<std::uint32_t>(value)};
      } else {
        return RuleValue{static_cast<std::uint64_t>(value)};
      }
    }
  } else {
    static_assert(sizeof(RawT) == 0, "Unsupported rule value type.");
  }
}

template <typename T> struct IsStdVariant : std::false_type {};
template <typename... Ts>
struct IsStdVariant<std::variant<Ts...>> : std::true_type {};

} // namespace pegium::parser::detail
