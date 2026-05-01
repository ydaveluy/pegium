/// Per-combinator candidate bound declarations.
///
/// Each combinator (OrderedChoice, Group, Repetition, InfixRule)
/// exposes `kMaxRecoveryCandidatesPerCall` as a `constexpr
/// std::size_t` derived structurally from the grammar arity. The
/// bound is pinned here by template instantiation against canonical
/// arities; any future change that drifts the bound expression away
/// from its declared structural form makes this suite fail loudly.
///
/// The bound expressions are verifiable by inspection: `OrderedChoice`
/// calls `consider_choice_attempt` at most `5N` times per
/// `recover()`, `Group` evaluates at most 4 candidates per dispatch
/// site, `Repetition` walks a 4-element `IterationPlanList`, and
/// `InfixRule` selects from a 2-element `EditableTailCandidate`
/// array.

#include "RecoveryTestSupport.hpp"

#include <pegium/core/parser/Group.hpp>
#include <pegium/core/parser/InfixRule.hpp>
#include <pegium/core/parser/Literal.hpp>
#include <pegium/core/parser/OrderedChoice.hpp>
#include <pegium/core/parser/PegiumParser.hpp>
#include <pegium/core/parser/Repetition.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>

using namespace pegium::parser;
using pegium::test::recovery::RecoveryBinaryExpression;

namespace {

// Concrete `Literal` types used to instantiate the combinator
// templates. The value of the literal is irrelevant to the bound;
// only the parameter pack arity matters.
using L1 = decltype("a"_kw);
using L2 = decltype("b"_kw);
using L3 = decltype("c"_kw);
using L4 = decltype("d"_kw);
using L5 = decltype("e"_kw);
using L6 = decltype("f"_kw);

// `InfixRule` requires `<T, &T::left, &T::op, &T::right>`. The
// member pointers are concrete; only `T` varies. The bound is
// independent of `T`, but we need at least one valid instantiation
// to pin the value.
using InfixOfBinary =
    InfixRule<RecoveryBinaryExpression, &RecoveryBinaryExpression::left,
              &RecoveryBinaryExpression::op,
              &RecoveryBinaryExpression::right>;

} // namespace

// -----------------------------------------------------------------------------
// 1. OrderedChoice: bound = 5 * sizeof...(Elements)
// -----------------------------------------------------------------------------

TEST(CandidateBound, ordered_choice_bound_is_five_times_arity) {
  using ChoiceOf2 = OrderedChoice<L1, L2>;
  using ChoiceOf3 = OrderedChoice<L1, L2, L3>;
  using ChoiceOf5 = OrderedChoice<L1, L2, L3, L4, L5>;
  static_assert(ChoiceOf2::kMaxRecoveryCandidatesPerCall == 10U);
  static_assert(ChoiceOf3::kMaxRecoveryCandidatesPerCall == 15U);
  static_assert(ChoiceOf5::kMaxRecoveryCandidatesPerCall == 25U);
  EXPECT_EQ(ChoiceOf2::kMaxRecoveryCandidatesPerCall, 10U);
  EXPECT_EQ(ChoiceOf3::kMaxRecoveryCandidatesPerCall, 15U);
  EXPECT_EQ(ChoiceOf5::kMaxRecoveryCandidatesPerCall, 25U);
}

// -----------------------------------------------------------------------------
// 2. Group: bound = 4 (per dispatch site, independent of arity)
// -----------------------------------------------------------------------------

TEST(CandidateBound, group_bound_is_four_per_dispatch_site) {
  // The bound is per-site, not cumulative. The recursive call
  // `parse_elements<I+1>` opens a fresh counter session so growth
  // across the sequence is bounded site-by-site.
  using GroupOf2 = Group<L1, L2>;
  using GroupOf3 = Group<L1, L2, L3>;
  using GroupOf6 = Group<L1, L2, L3, L4, L5, L6>;
  static_assert(GroupOf2::kMaxRecoveryCandidatesPerCall == 4U);
  static_assert(GroupOf3::kMaxRecoveryCandidatesPerCall == 4U);
  static_assert(GroupOf6::kMaxRecoveryCandidatesPerCall == 4U);
  EXPECT_EQ(GroupOf2::kMaxRecoveryCandidatesPerCall, 4U);
  EXPECT_EQ(GroupOf3::kMaxRecoveryCandidatesPerCall, 4U);
  EXPECT_EQ(GroupOf6::kMaxRecoveryCandidatesPerCall, 4U);
}

// -----------------------------------------------------------------------------
// 3. Repetition: bound = 4 (constant — IterationPlanList size)
// -----------------------------------------------------------------------------

TEST(CandidateBound, repetition_bound_is_four_independent_of_min_max) {
  // Every flavor of Repetition (optional, star, plus, fixed) shares
  // the same bound, since the bound comes from `IterationPlanList`'s
  // static array size, not from min/max.
  using Optional = Repetition<0, 1, L1>;
  using Star =
      Repetition<0, std::numeric_limits<std::size_t>::max(), L1>;
  using Plus =
      Repetition<1, std::numeric_limits<std::size_t>::max(), L1>;
  using FixedThree = Repetition<3, 3, L1>;
  static_assert(Optional::kMaxRecoveryCandidatesPerCall == 4U);
  static_assert(Star::kMaxRecoveryCandidatesPerCall == 4U);
  static_assert(Plus::kMaxRecoveryCandidatesPerCall == 4U);
  static_assert(FixedThree::kMaxRecoveryCandidatesPerCall == 4U);
  EXPECT_EQ(Optional::kMaxRecoveryCandidatesPerCall, 4U);
  EXPECT_EQ(Star::kMaxRecoveryCandidatesPerCall, 4U);
  EXPECT_EQ(Plus::kMaxRecoveryCandidatesPerCall, 4U);
  EXPECT_EQ(FixedThree::kMaxRecoveryCandidatesPerCall, 4U);
}

// -----------------------------------------------------------------------------
// 4. InfixRule: bound = 2 (constant — EditableTailCandidate array size)
// -----------------------------------------------------------------------------

TEST(CandidateBound, infix_bound_is_two_independent_of_arity) {
  // The bound is a property of the editable-tail decision, not of
  // the operator count or the AST tag. The static assertion holds
  // for the canonical binary-expression InfixRule instantiation.
  static_assert(InfixOfBinary::kMaxRecoveryCandidatesPerCall == 2U);
  EXPECT_EQ(InfixOfBinary::kMaxRecoveryCandidatesPerCall, 2U);
}

// -----------------------------------------------------------------------------
// 5. Bounds are positive and grammar-derived (sanity floor)
// -----------------------------------------------------------------------------

TEST(CandidateBound, every_combinator_declares_a_strictly_positive_bound) {
  // Floor invariant: a bound of zero would mean "no candidate ever"
  // — that is incompatible with recovery being able to choose any
  // attempt. A future refactor that mistakenly sets the bound to 0
  // (e.g., wrong dependence on some flag) is caught here.
  EXPECT_GT((Group<L1, L2>::kMaxRecoveryCandidatesPerCall), 0U);
  EXPECT_GT((OrderedChoice<L1, L2>::kMaxRecoveryCandidatesPerCall), 0U);
  EXPECT_GT((Repetition<0, 1, L1>::kMaxRecoveryCandidatesPerCall), 0U);
  EXPECT_GT(InfixOfBinary::kMaxRecoveryCandidatesPerCall, 0U);
}

