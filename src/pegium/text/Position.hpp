#pragma once

#include <concepts>
#include <cstdint>
#include <type_traits>

namespace pegium::text {

struct Position {
  std::uint32_t line = 0;
  std::uint32_t character = 0;

  constexpr Position() noexcept = default;
  constexpr Position(std::uint32_t line, std::uint32_t character) noexcept
      : line(line), character(character) {}

  template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Position> &&
             requires(const std::remove_reference_t<T> &value) {
               { value.line } -> std::convertible_to<std::uint32_t>;
               { value.character } -> std::convertible_to<std::uint32_t>;
             })
  constexpr Position(const T &value) noexcept
      : line(static_cast<std::uint32_t>(value.line)),
        character(static_cast<std::uint32_t>(value.character)) {}

  template <typename T>
    requires requires(T value) {
      value.line = std::uint32_t{};
      value.character = std::uint32_t{};
    }
  constexpr operator T() const {
    T value{};
    value.line = line;
    value.character = character;
    return value;
  }
};

} // namespace pegium::text
