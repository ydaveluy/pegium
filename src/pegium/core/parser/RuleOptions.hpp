#pragma once

/// Option types used when configuring parser rules and converters.

#include <concepts>
#include <optional>
#include <pegium/core/parser/Skipper.hpp>
#include <string_view>
#include <type_traits>
#include <utility>

namespace pegium::parser::opt {

struct SkipperOption {
  Skipper skipper;
};

template <typename Converter> struct ConverterOption {
  Converter converter;
};

struct ConversionErrorTag {
  explicit constexpr ConversionErrorTag() noexcept = default;
};

inline constexpr ConversionErrorTag conversion_error_tag{};

template <typename T> struct ConversionResult {
  using value_type = T;

  std::optional<T> _value{};
  std::string_view _error{};

  template <typename... Args>
  constexpr explicit ConversionResult(std::in_place_t, Args &&...args) noexcept(
      std::is_nothrow_constructible_v<T, Args &&...>)
      : _value(std::in_place, std::forward<Args>(args)...), _error{} {}

  constexpr explicit ConversionResult(ConversionErrorTag,
                                      std::string_view error) noexcept
      : _value(std::nullopt), _error(error) {}

  [[nodiscard]] constexpr bool has_value() const noexcept {
    return _value.has_value();
  }

  constexpr explicit operator bool() const noexcept { return has_value(); }

  [[nodiscard]] constexpr const T &value() const & noexcept { return *_value; }
  [[nodiscard]] constexpr T &value() & noexcept { return *_value; }
  [[nodiscard]] constexpr T &&value() && noexcept { return std::move(*_value); }

  [[nodiscard]] constexpr std::string_view error() const noexcept {
    return _error;
  }
};

template <typename Option> struct IsSkipperOption : std::false_type {};
template <> struct IsSkipperOption<SkipperOption> : std::true_type {};

template <typename Option>
inline constexpr bool IsSkipperOption_v =
    IsSkipperOption<std::remove_cvref_t<Option>>::value;

template <typename Option> struct IsConverterOption : std::false_type {};
template <typename Converter>
struct IsConverterOption<ConverterOption<Converter>> : std::true_type {};

template <typename Option>
inline constexpr bool IsConverterOption_v =
    IsConverterOption<std::remove_cvref_t<Option>>::value;

template <typename Result> struct IsConversionResult : std::false_type {};
template <typename T>
struct IsConversionResult<ConversionResult<T>> : std::true_type {};

template <typename Result>
inline constexpr bool IsConversionResult_v =
    IsConversionResult<std::remove_cvref_t<Result>>::value;

template <typename Result> struct ConversionResultTraits {
  using value_type = void;
};
template <typename T> struct ConversionResultTraits<ConversionResult<T>> {
  using value_type = T;
};

template <typename Result>
using ConversionResultValue_t =
    typename ConversionResultTraits<std::remove_cvref_t<Result>>::value_type;

template <typename Result, typename T>
inline constexpr bool IsConversionResultFor_v =
    IsConversionResult_v<Result> &&
    std::convertible_to<ConversionResultValue_t<Result>, T>;

template <typename SkipperType>
  requires std::same_as<std::remove_cvref_t<SkipperType>, Skipper>
constexpr auto with_skipper(SkipperType &&skipper) {
  return SkipperOption{std::forward<SkipperType>(skipper)};
}

template <typename Converter>
constexpr auto with_converter(Converter &&converter) {
  return ConverterOption<std::remove_cvref_t<Converter>>{
      std::forward<Converter>(converter)};
}

template <typename T, typename... Args>
constexpr auto conversion_value(Args &&...args) noexcept(
    std::is_nothrow_constructible_v<T, Args &&...>) {
  return ConversionResult<T>{std::in_place, std::forward<Args>(args)...};
}

template <typename T>
constexpr auto conversion_error(std::string_view message) noexcept {
  return ConversionResult<T>{conversion_error_tag, message};
}

namespace detail {

template <typename T> struct DependentFalse : std::false_type {};
template <typename T>
inline constexpr bool DependentFalse_v = DependentFalse<T>::value;

} // namespace detail

} // namespace pegium::parser::opt
