#pragma once

#include <cstdint>
#include <string_view>

namespace pegium::lsp {

class FuzzyMatcher {
public:
  virtual ~FuzzyMatcher() noexcept = default;

  [[nodiscard]] virtual bool
  match(std::string_view query, std::string_view text) const = 0;
};

} // namespace pegium::lsp
