#pragma once

#include <algorithm>
#include <concepts>
#include <utility>
#include <vector>

namespace pegium::utils {

template <typename Container, typename Value>
requires requires(const Container &container, const Value &value) {
  container.contains(value);
}
/// Returns whether `container.contains(value)` succeeds.
[[nodiscard]] bool contains(const Container &container, const Value &value) {
  return container.contains(value);
}

template <typename Container, typename Value>
requires requires(const Container &container, const Value &value) {
  container.begin();
  container.end();
  *container.begin() == value;
}
/// Returns whether `value` is present in a linearly scanned container.
[[nodiscard]] bool contains_linear(const Container &container,
                                   const Value &value) {
  return std::ranges::find(container, value) != container.end();
}

template <typename AssocContainer>
requires requires(const AssocContainer &container) {
  container.begin();
  container.end();
  container.begin()->first;
}
/// Returns a copy of the keys stored in an associative container.
[[nodiscard]] auto keys(const AssocContainer &container) {
  using KeyType = std::decay_t<decltype(container.begin()->first)>;
  std::vector<KeyType> out;
  out.reserve(container.size());
  for (const auto &[key, value] : container) {
    (void)value;
    out.push_back(key);
  }
  return out;
}

template <typename AssocContainer>
requires requires(const AssocContainer &container) {
  container.begin();
  container.end();
  container.begin()->second;
}
/// Returns a copy of the values stored in an associative container.
[[nodiscard]] auto values(const AssocContainer &container) {
  using ValueType = std::decay_t<decltype(container.begin()->second)>;
  std::vector<ValueType> out;
  out.reserve(container.size());
  for (const auto &[key, value] : container) {
    (void)key;
    out.push_back(value);
  }
  return out;
}

} // namespace pegium::utils
