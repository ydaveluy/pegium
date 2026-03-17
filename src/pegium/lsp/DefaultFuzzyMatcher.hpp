#pragma once

#include <pegium/lsp/FuzzyMatcher.hpp>

namespace pegium::lsp {

class DefaultFuzzyMatcher : public FuzzyMatcher {
public:
  [[nodiscard]] bool
  match(std::string_view query, std::string_view text) const override;
};

} // namespace pegium::lsp
