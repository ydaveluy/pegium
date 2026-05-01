/// `LexicalRecoveryProfile` <-> `TerminalShape` projection
/// equivalence.
///
/// `TerminalShape` is the canonical lexical origin and
/// `LexicalRecoveryProfile` is an alias of it. The legacy
/// `classify_literal_recovery_profile` returns the canonical shape
/// directly, with the boundary flags left at `false` (existing
/// callers consume only the three "legacy subset" fields:
/// `hasCanonicalText`, `singleCodepoint`, `canonicalTextLength`).
///
/// This test pins the equivalence: for every input,
/// `classify_literal_recovery_profile(v)` MUST match
/// `make_terminal_shape_from_literal(v, false, false)` on the three
/// legacy-subset fields. If a future change drifts one without the
/// other, the single-source-of-truth guarantee breaks and this test
/// fails immediately.

#include <pegium/core/parser/TerminalRecoverySupport.hpp>
#include <pegium/core/parser/TerminalShape.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string_view>

using pegium::parser::detail::classify_literal_recovery_profile;
using pegium::parser::detail::make_terminal_shape_from_literal;
using pegium::parser::detail::TerminalShape;

namespace {

[[nodiscard]] bool legacy_subset_equal(const TerminalShape &a,
                                       const TerminalShape &b) noexcept {
  return a.hasCanonicalText == b.hasCanonicalText &&
         a.singleCodepoint == b.singleCodepoint &&
         a.canonicalTextLength == b.canonicalTextLength;
}

} // namespace

TEST(TerminalShapeProjection,
     classifier_matches_shape_builder_on_canonical_inputs) {
  // Cover empty, single-codepoint punctuation, multi-codepoint word
  // keywords, and multi-codepoint punctuation. A regression on any
  // shape category surfaces here with the offending value.
  static constexpr std::array kInputs = {
      std::string_view{""},      std::string_view{";"},
      std::string_view{":"},     std::string_view{"="},
      std::string_view{"if"},    std::string_view{"service"},
      std::string_view{"requirement"}, std::string_view{"=>"},
      std::string_view{"=="},    std::string_view{"..."}};
  for (const auto value : kInputs) {
    SCOPED_TRACE(testing::Message() << "value='" << value << "'");
    const auto fromClassify = classify_literal_recovery_profile(value);
    const auto fromBuilder = make_terminal_shape_from_literal(
        value, /*startsLikeWord=*/false, /*endsLikeWord=*/false);
    EXPECT_TRUE(legacy_subset_equal(fromClassify, fromBuilder));
  }
}

TEST(TerminalShapeProjection,
     boundary_flags_do_not_affect_legacy_subset_equivalence) {
  // The legacy subset is the three fields existing callers read; the
  // two boundary flags are independent. Building a shape with
  // boundary flags set must not perturb the legacy subset.
  const auto fromClassify = classify_literal_recovery_profile("service");
  const auto withFlags = make_terminal_shape_from_literal(
      "service", /*startsLikeWord=*/true, /*endsLikeWord=*/true);
  EXPECT_TRUE(legacy_subset_equal(fromClassify, withFlags));
}
