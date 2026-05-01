#pragma once

/// Reusable cache-on/cache-off neutrality harness.
///
/// The `ChoiceRecoverCache` is a pure optimization: disabling it must
/// never change the chosen recovery candidate. Tests that touch
/// policy or the choice pipeline use this harness so the cache
/// neutrality invariant is asserted uniformly across the test base.
///
/// Usage:
///   pegium::test::expect_cache_neutral(
///       [&](const ParseOptions &opts) {
///           return parseRule(rule, input, skipper, opts);
///       },
///       "case_name", input);
///
/// The callable must accept a `ParseOptions` and return any structure
/// that exposes the standard `ParsedResult` fields (`fullMatch`,
/// `parsedLength`, `parseDiagnostics`). The harness runs the callable
/// twice — once with the cache enabled, once with it disabled — and
/// asserts that every observable output is identical.

#include <cstddef>
#include <string>
#include <string_view>

#include <gtest/gtest.h>
#include <pegium/core/parser/Parser.hpp>

namespace pegium::test {

namespace detail {

inline std::string
dump_cache_neutrality_diagnostics(const std::vector<parser::ParseDiagnostic> &diagnostics) {
  std::string dump;
  for (const auto &diagnostic : diagnostics) {
    if (!dump.empty()) {
      dump += " | ";
    }
    dump += std::to_string(diagnostic.beginOffset);
    dump += "-";
    dump += std::to_string(diagnostic.endOffset);
    dump += ":";
    switch (diagnostic.kind) {
    case parser::ParseDiagnosticKind::Inserted:
      dump += "Inserted";
      break;
    case parser::ParseDiagnosticKind::Deleted:
      dump += "Deleted";
      break;
    case parser::ParseDiagnosticKind::Replaced:
      dump += "Replaced";
      break;
    default:
      dump += "Other";
      break;
    }
  }
  return dump;
}

} // namespace detail

template <typename ParseFn>
void expect_cache_neutral(ParseFn parse, std::string_view caseName,
                          std::string_view input) {
  parser::ParseOptions cacheOnOptions;
  parser::ParseOptions cacheOffOptions;
  cacheOffOptions.diagnostics.recoveryCacheDisabled = true;

  auto onResult = parse(cacheOnOptions);
  auto offResult = parse(cacheOffOptions);

  SCOPED_TRACE(testing::Message() << "case=" << caseName << " input=\"" << input
                                  << "\"");

  EXPECT_EQ(onResult.fullMatch, offResult.fullMatch)
      << "fullMatch diverges between cache-on and cache-off";
  EXPECT_EQ(onResult.parsedLength, offResult.parsedLength)
      << "parsedLength diverges between cache-on and cache-off";

  const auto onDump =
      detail::dump_cache_neutrality_diagnostics(onResult.parseDiagnostics);
  const auto offDump =
      detail::dump_cache_neutrality_diagnostics(offResult.parseDiagnostics);
  ASSERT_EQ(onResult.parseDiagnostics.size(), offResult.parseDiagnostics.size())
      << "diagnostic count diverges\n  cache-on : " << onDump
      << "\n  cache-off: " << offDump;
  for (std::size_t i = 0; i < onResult.parseDiagnostics.size(); ++i) {
    const auto &a = onResult.parseDiagnostics[i];
    const auto &b = offResult.parseDiagnostics[i];
    EXPECT_EQ(a.kind, b.kind)
        << "diagnostic[" << i << "].kind diverges\n  cache-on : " << onDump
        << "\n  cache-off: " << offDump;
    EXPECT_EQ(a.beginOffset, b.beginOffset)
        << "diagnostic[" << i
        << "].beginOffset diverges\n  cache-on : " << onDump
        << "\n  cache-off: " << offDump;
    EXPECT_EQ(a.endOffset, b.endOffset)
        << "diagnostic[" << i
        << "].endOffset diverges\n  cache-on : " << onDump
        << "\n  cache-off: " << offDump;
  }

  // Sanity: at least one of the two paths must have actually entered a
  // recovery state (otherwise the test only proves cache neutrality on a
  // strict-only parse, which is trivial).
  const bool exercised = !onResult.parseDiagnostics.empty() ||
                         !offResult.parseDiagnostics.empty() ||
                         !onResult.fullMatch || !offResult.fullMatch;
  EXPECT_TRUE(exercised)
      << "case did not trigger any recovery — input is too clean to be a "
         "meaningful neutrality probe";
}

} // namespace pegium::test
