#pragma once

/// Generic harness for keyword/symbol fuzz sweeps over an example
/// language. A language's fuzz file declares the base text + symbol
/// set + keyword list, then exposes them via a small descriptor; the
/// templated `KeywordFuzzRunner` walks every single mutation, every
/// pair, and a deterministic random sample of triples / larger combos.
///
/// Mutation primitives:
///   - `DropChar`: delete one byte at `position` (used for symbols).
///   - `TruncateKeyword`: delete the trailing byte of a keyword
///     occurrence — the canonical "missing-codepoint" typo class
///     recovery is required to repair (`entit -> entity`,
///     `extend -> extends`, etc.).
///
/// Each language registers a ctest entry gated on its own env var
/// (e.g. `PEGIUM_DOMAINMODEL_FUZZ_EXHAUSTIVE=1`). The harness only
/// runs when the env var is set; otherwise tests `GTEST_SKIP`.

#include <gtest/gtest.h>

#include <pegium/examples/ExampleTestSupport.hpp>
#include <pegium/examples/RecoverySampleTestSupport.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pegium::test::keyword_fuzz {

struct Mutation {
  enum class Kind : std::uint8_t { DropChar, TruncateKeyword };

  Kind kind = Kind::DropChar;
  std::size_t position = 0;
  std::size_t length = 0;
  std::string_view label{};

  [[nodiscard]] bool operator<(const Mutation &other) const noexcept {
    return position < other.position;
  }
};

/// Returns true if `c` could be part of an identifier (letter, digit,
/// underscore). Used to confirm a keyword occurrence is bounded by
/// non-identifier characters before flagging it as a TruncateKeyword
/// candidate.
[[nodiscard]] inline bool is_ident_char(char c) noexcept {
  return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

[[nodiscard]] inline std::vector<Mutation>
collect_mutations(std::string_view baseText,
                   std::string_view droppableSymbols,
                   std::span<const std::string_view> keywords) {
  std::vector<Mutation> mutations;
  for (std::size_t i = 0; i < baseText.size(); ++i) {
    if (droppableSymbols.find(baseText[i]) != std::string_view::npos) {
      mutations.push_back(
          {Mutation::Kind::DropChar, i, 1,
           std::string_view{baseText.data() + i, 1}});
    }
  }
  for (const auto keyword : keywords) {
    std::size_t cursor = 0;
    while (true) {
      const auto pos = baseText.find(keyword, cursor);
      if (pos == std::string_view::npos) {
        break;
      }
      const bool boundedLeft =
          pos == 0 || !is_ident_char(baseText[pos - 1]);
      const auto endPos = pos + keyword.size();
      const bool boundedRight =
          endPos == baseText.size() || !is_ident_char(baseText[endPos]);
      if (boundedLeft && boundedRight) {
        mutations.push_back({Mutation::Kind::TruncateKeyword,
                              endPos - 1, 1, keyword});
      }
      cursor = endPos;
    }
  }
  return mutations;
}

[[nodiscard]] inline std::string
apply_mutations(std::string_view baseText,
                std::vector<Mutation> mutations) {
  std::sort(mutations.begin(), mutations.end());
  // Reject overlapping mutations (random sweeps may pick keyword
  // positions that overlap each other or a symbol drop). Returning
  // the empty string skips the case.
  for (std::size_t i = 1; i < mutations.size(); ++i) {
    const auto prevEnd =
        mutations[i - 1].position + mutations[i - 1].length;
    if (mutations[i].position < prevEnd) {
      return {};
    }
  }
  std::string result;
  result.reserve(baseText.size());
  std::size_t cursor = 0;
  for (const auto &m : mutations) {
    result.append(baseText, cursor, m.position - cursor);
    cursor = m.position + m.length;
  }
  result.append(baseText, cursor, baseText.size() - cursor);
  return result;
}

[[nodiscard]] inline std::string
describe_mutations(const std::vector<Mutation> &mutations) {
  std::string out = "mutate{";
  for (std::size_t i = 0; i < mutations.size(); ++i) {
    if (i > 0) {
      out += ',';
    }
    out += std::to_string(mutations[i].position);
    out += '(';
    out += mutations[i].kind == Mutation::Kind::DropChar ? "drop:" : "trunc:";
    out += mutations[i].label;
    out += ')';
  }
  out += '}';
  return out;
}

inline void enumerate_combinations(
    const std::vector<Mutation> &mutations, std::size_t k,
    const std::function<void(const std::vector<Mutation> &)> &visit) {
  std::vector<Mutation> pick(k);
  std::function<void(std::size_t, std::size_t)> rec =
      [&](std::size_t start, std::size_t depth) {
        if (depth == k) {
          visit(pick);
          return;
        }
        for (std::size_t i = start;
             i + (k - depth) <= mutations.size(); ++i) {
          pick[depth] = mutations[i];
          rec(i + 1, depth + 1);
        }
      };
  rec(0, 0);
}

[[nodiscard]] inline bool
fuzz_sweeps_enabled(const char *envVarName) noexcept {
  const char *v = std::getenv(envVarName);
  return v != nullptr && v[0] != '\0' && v[0] != '0';
}

template <typename ParserFactory>
void expect_recovered(ParserFactory &parserFactory,
                       std::string_view baseText,
                       std::string_view fileSuffix,
                       std::string_view languageId,
                       const std::vector<Mutation> &mutations) {
  const auto text = apply_mutations(baseText, mutations);
  if (text.empty()) {
    return; // skipped: overlapping mutations
  }
  std::string uriBase{"keyword-fuzz"};
  uriBase += fileSuffix;
  auto &parser = parserFactory();
  auto document = pegium::test::parse_document(
      parser, text, pegium::test::make_file_uri(uriBase),
      std::string{languageId});
  const auto &parsed = document->parseResult;
  const auto parseDump =
      pegium::test::dump_parse_diagnostics(parsed.parseDiagnostics);
  const auto label = describe_mutations(mutations);
  EXPECT_TRUE(parsed.value.get() != nullptr)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
  EXPECT_TRUE(parsed.fullMatch)
      << label << "\n----\n" << text << "\n----\n" << parseDump;
}

template <typename ParserFactory>
void run_single_sweep(ParserFactory &parserFactory,
                       std::string_view baseText,
                       std::string_view droppableSymbols,
                       std::span<const std::string_view> keywords,
                       std::string_view fileSuffix,
                       std::string_view languageId) {
  const auto mutations =
      collect_mutations(baseText, droppableSymbols, keywords);
  enumerate_combinations(mutations, 1u,
                          [&](const std::vector<Mutation> &pick) {
                            expect_recovered(parserFactory, baseText,
                                             fileSuffix, languageId, pick);
                          });
}

template <typename ParserFactory>
void run_pair_sweep(ParserFactory &parserFactory,
                     std::string_view baseText,
                     std::string_view droppableSymbols,
                     std::span<const std::string_view> keywords,
                     std::string_view fileSuffix,
                     std::string_view languageId) {
  const auto mutations =
      collect_mutations(baseText, droppableSymbols, keywords);
  enumerate_combinations(mutations, 2u,
                          [&](const std::vector<Mutation> &pick) {
                            expect_recovered(parserFactory, baseText,
                                             fileSuffix, languageId, pick);
                          });
}

template <typename ParserFactory>
void run_random_triples_sweep(ParserFactory &parserFactory,
                                std::string_view baseText,
                                std::string_view droppableSymbols,
                                std::span<const std::string_view> keywords,
                                std::string_view fileSuffix,
                                std::string_view languageId,
                                std::uint32_t seed, int iterations) {
  const auto mutations =
      collect_mutations(baseText, droppableSymbols, keywords);
  if (mutations.size() < 3u) {
    return;
  }
  std::mt19937 rng(seed);
  for (int i = 0; i < iterations; ++i) {
    std::vector<Mutation> shuffled = mutations;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(3);
    expect_recovered(parserFactory, baseText, fileSuffix, languageId,
                      shuffled);
  }
}

template <typename ParserFactory>
void run_random_larger_sweep(ParserFactory &parserFactory,
                              std::string_view baseText,
                              std::string_view droppableSymbols,
                              std::span<const std::string_view> keywords,
                              std::string_view fileSuffix,
                              std::string_view languageId,
                              std::uint32_t seed, int iterations) {
  const auto mutations =
      collect_mutations(baseText, droppableSymbols, keywords);
  if (mutations.size() < 3u) {
    return;
  }
  std::mt19937 rng(seed);
  for (int i = 0; i < iterations; ++i) {
    const auto upperBound = std::min<std::size_t>(mutations.size(), 8);
    const auto k = std::uniform_int_distribution<std::size_t>(3, upperBound)(rng);
    std::vector<Mutation> shuffled = mutations;
    std::shuffle(shuffled.begin(), shuffled.end(), rng);
    shuffled.resize(k);
    expect_recovered(parserFactory, baseText, fileSuffix, languageId,
                      shuffled);
  }
}

} // namespace pegium::test::keyword_fuzz
