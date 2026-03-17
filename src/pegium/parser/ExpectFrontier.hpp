#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

#include <pegium/parser/ExpectContext.hpp>
#include <pegium/parser/ParseAttempt.hpp>

namespace pegium::parser {

struct CapturedFrontier {
  bool blocked = false;
  std::vector<ExpectPath> items;
};

struct ExpectBranchResult {
  bool matched = false;
  bool blocked = false;
  TextOffset cursor = 0;
  std::uint32_t editCost = 0;
  std::vector<ExpectPath> frontier;
};

[[gnu::always_inline]] inline CapturedFrontier
capture_frontier_since(ExpectContext &ctx,
                       const ExpectContext::Checkpoint &checkpoint) {
  CapturedFrontier frontier{.blocked = ctx.frontierBlocked()};
  if (checkpoint.frontierSize >= ctx.frontier.size()) {
    return frontier;
  }
  const auto first =
      ctx.frontier.begin() + static_cast<std::ptrdiff_t>(checkpoint.frontierSize);
  frontier.items.reserve(ctx.frontier.size() - checkpoint.frontierSize);
  frontier.items.insert(frontier.items.end(), first, ctx.frontier.end());
  return frontier;
}

[[gnu::always_inline]] inline void
merge_captured_frontier(ExpectContext &ctx, const CapturedFrontier &frontier,
                        bool clear_block_if_unblocked) {
  if (frontier.items.empty()) {
    if (frontier.blocked) {
      ctx.blockFrontier();
    }
  } else {
    ctx.mergeFrontier(frontier.items);
  }
  if (clear_block_if_unblocked) {
    ctx.clearFrontierBlock();
  }
}

template <Expression E>
[[gnu::always_inline]] inline bool replay_expect_branch(ExpectContext &ctx,
                                                        const E &expression) {
  if constexpr (requires_checkpoint_on_failure_v<E>) {
    const auto checkpoint = ctx.mark();
    if (parse(expression, ctx)) {
      return true;
    }
    ctx.rewind(checkpoint);
    return false;
  } else {
    return parse(expression, ctx);
  }
}

template <Expression E>
[[gnu::always_inline]] inline void
collect_expect_branch(ExpectContext &ctx, const ExpectContext::Checkpoint &base,
                      const E &expression, ExpectBranchResult &result) {
  result = {};
  ctx.rewind(base);
  const auto checkpoint = ctx.mark();
  if (!replay_expect_branch(ctx, expression)) {
    ctx.rewind(base);
    return;
  }

  auto frontier = capture_frontier_since(ctx, checkpoint);
  result.matched = true;
  result.blocked = frontier.blocked;
  result.cursor = ctx.cursorOffset();
  result.editCost = ctx.currentEditCost();
  result.frontier = std::move(frontier.items);
}

} // namespace pegium::parser
