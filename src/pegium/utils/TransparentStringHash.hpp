#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

namespace pegium::utils {

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

template <typename T>
using TransparentStringMap =
    std::unordered_map<std::string, T, TransparentStringHash, std::equal_to<>>;

using TransparentStringSet =
    std::unordered_set<std::string, TransparentStringHash, std::equal_to<>>;

} // namespace pegium::utils
