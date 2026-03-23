#pragma once

#include <concepts>
#include <type_traits>

#include <pegium/core/text/Position.hpp>

namespace pegium::text {

struct Range;

template <typename T>
concept RangeLike =
    !std::same_as<std::remove_cvref_t<T>, Range> &&
    requires(const std::remove_reference_t<T> &value) {
      Position(value.start);
      Position(value.end);
    };

template <typename T>
concept MutableRangeLike = requires(T value, Position position) {
  value.start = position;
  value.end = position;
};

/// Half-open text range expressed with UTF-16 positions.
struct Range {
  Position start;
  Position end;

  constexpr Range() noexcept = default;
  constexpr Range(Position start, Position end) noexcept
      : start(start), end(end) {}

  template <RangeLike T>
  explicit(false) constexpr Range(const T &value) noexcept
      : start(value.start), end(value.end) {}

  template <MutableRangeLike T>
  explicit(false) constexpr operator T() const {
    T value{};
    value.start = start;
    value.end = end;
    return value;
  }
};

} // namespace pegium::text
