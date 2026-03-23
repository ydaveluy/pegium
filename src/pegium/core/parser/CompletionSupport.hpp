#pragma once

/// Completion-specific extension points exposed by parser expressions.

namespace pegium::parser {

struct Skipper;

struct CompletionSkipperProvider {
  virtual ~CompletionSkipperProvider() noexcept = default;

  [[nodiscard]] virtual const Skipper *
  getCompletionSkipper() const noexcept = 0;
};

struct CompletionTerminalMatcher {
  virtual ~CompletionTerminalMatcher() noexcept = default;

  [[nodiscard]] virtual const char *
  matchForCompletion(const char *begin) const noexcept = 0;
};

} // namespace pegium::parser
