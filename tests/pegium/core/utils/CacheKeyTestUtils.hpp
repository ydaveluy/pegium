#pragma once

#include <cstddef>
#include <functional>
#include <string>

namespace pegium::utils::test_support {

struct NamedKey {
  std::string value;
};

struct NamedKeyHash {
  [[nodiscard]] std::size_t operator()(const NamedKey &key) const noexcept {
    return std::hash<std::string>{}(key.value);
  }
};

struct NamedKeyEqual {
  [[nodiscard]] bool operator()(const NamedKey &lhs,
                                const NamedKey &rhs) const noexcept {
    return lhs.value == rhs.value;
  }
};

} // namespace pegium::utils::test_support
