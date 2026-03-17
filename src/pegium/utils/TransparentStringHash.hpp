#pragma once

#include <cstddef>
#include <string>
#include <string_view>

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

} // namespace pegium::utils
