#pragma once

#include <string>
#include <vector>

namespace pegium::validation {

struct ValidationOptions {
  bool enabled = true;
  std::vector<std::string> categories;

  [[nodiscard]] bool empty() const noexcept { return categories.empty(); }
};

} // namespace pegium::validation
