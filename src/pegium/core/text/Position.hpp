#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace pegium::text {

struct Position;

template <typename T>
concept PositionLike =
    !std::same_as<std::remove_cvref_t<T>, Position> &&
    requires(const std::remove_reference_t<T> &value) {
      { value.line } -> std::convertible_to<std::uint32_t>;
      { value.character } -> std::convertible_to<std::uint32_t>;
    };

template <typename T>
concept MutablePositionLike = requires(T value, std::uint32_t coordinate) {
  value.line = coordinate;
  value.character = coordinate;
};

/// Zero-based text position expressed in UTF-16 code units.
struct Position {
  std::uint32_t line = 0;
  std::uint32_t character = 0;

  constexpr Position() noexcept = default;
  constexpr Position(std::uint32_t line, std::uint32_t character) noexcept
      : line(line), character(character) {}

  template <PositionLike T>
  explicit(false) constexpr Position(const T &value) noexcept
      : line(static_cast<std::uint32_t>(value.line)),
        character(static_cast<std::uint32_t>(value.character)) {}

  template <MutablePositionLike T>
  explicit(false) constexpr operator T() const {
    T value{};
    value.line = line;
    value.character = character;
    return value;
  }
};

} // namespace pegium::text
