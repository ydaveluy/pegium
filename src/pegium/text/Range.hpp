#pragma once

#include <concepts>
#include <type_traits>

#include <pegium/text/Position.hpp>

namespace pegium::text {

struct Range {
  Position start;
  Position end;

  constexpr Range() noexcept = default;
  constexpr Range(Position start, Position end) noexcept
      : start(start), end(end) {}

  template <typename T>
    requires(!std::same_as<std::remove_cvref_t<T>, Range> &&
             requires(const std::remove_reference_t<T> &value) {
               Position(value.start);
               Position(value.end);
             })
  constexpr Range(const T &value) noexcept
      : start(value.start), end(value.end) {}

  template <typename T>
    requires requires(T value, Position position) {
      value.start = position;
      value.end = position;
    }
  constexpr operator T() const {
    T value{};
    value.start = start;
    value.end = end;
    return value;
  }
};

} // namespace pegium::text
