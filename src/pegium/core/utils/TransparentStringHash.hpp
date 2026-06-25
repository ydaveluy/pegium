#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace pegium::utils {

/// Heterogeneous hash functor for `std::string`, `std::string_view`, and C strings.
struct TransparentStringHash {
  using is_transparent = void;

  [[nodiscard]] std::size_t operator()(std::string_view value) const noexcept {
    return std::hash<std::string_view>{}(value);
  }

  [[nodiscard]] std::size_t operator()(const std::string &value) const noexcept {
    return (*this)(std::string_view(value));
  }

  [[nodiscard]] std::size_t operator()(const char *value) const noexcept {
    return (*this)(std::string_view(value));
  }
};

/// A `string_view` bundled with its precomputed hash, for use as an unordered-map
/// key when the same string is probed many times (e.g. one reference name looked
/// up across every scope bucket and ancestor level). The hash is computed once
/// here and returned by `HashedStringViewHash`, so the map never re-scans the
/// bytes on `find`/insert. The constructors are implicit by design: insert/lookup
/// call sites keep passing a plain `std::string`/`std::string_view`, and only the
/// hot path constructs the key once and reuses it across probes.
struct HashedStringView {
  std::string_view value;
  std::size_t hash;

  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  HashedStringView(std::string_view view) noexcept
      : value(view), hash(std::hash<std::string_view>{}(view)) {}
  // NOLINTNEXTLINE(google-explicit-constructor,hicpp-explicit-conversions)
  HashedStringView(const std::string &str) noexcept
      : HashedStringView(std::string_view{str}) {}
};

struct HashedStringViewHash {
  [[nodiscard]] std::size_t
  operator()(const HashedStringView &key) const noexcept {
    return key.hash;
  }
};

struct HashedStringViewEqual {
  [[nodiscard]] bool operator()(const HashedStringView &lhs,
                                const HashedStringView &rhs) const noexcept {
    return lhs.value == rhs.value;
  }
};

/// Unordered map whose key caches its own hash (see `HashedStringView`).
template <typename T>
using HashedStringViewMap =
    std::unordered_map<HashedStringView, T, HashedStringViewHash,
                       HashedStringViewEqual>;

template <typename T>
using TransparentStringMap =
    std::unordered_map<std::string, T, TransparentStringHash, std::equal_to<>>;

/// Transparent unordered set of strings.
using TransparentStringSet =
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>;

} // namespace pegium::utils
