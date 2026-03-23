#pragma once

#include <pegium/lsp/support/FuzzyMatcher.hpp>

namespace pegium {

/// Default fuzzy matcher used by interactive LSP features.
class DefaultFuzzyMatcher : public FuzzyMatcher {
public:
  /// Returns whether `text` matches `query` according to fuzzy rules.
  [[nodiscard]] bool
  match(std::string_view query, std::string_view text) const override;
};

} // namespace pegium
