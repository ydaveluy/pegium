# Parser Contracts

This page documents the parser runtime contracts used by Pegium's expression
engine.

## Core terms

- `parse(expr, ctx)`: executes the expression in the given mode and may consume
  input, build CST nodes, and apply recovery edits depending on the context.
- `probe(expr, ctx)`: strict-mode lookahead that is guaranteed to be side
  effect free. It must not change the cursor, `maxCursor`, CST, or edit state.
- `fast_probe`: internal optimization hook used by hot paths such as operator
  matching. It is not part of the public API and carries no purity guarantee.
- `failure-safe`: an expression is failure-safe when a failed `parse(...)`
  leaves the caller's CST and cursor state reusable without an explicit rewind.
- `recover`: editable parsing mode driven by `RecoveryContext`.
- `expect`: frontier exploration mode driven by `ExpectContext`.
- `frontier`: the set of reachable completion or expectation paths collected by
  `ExpectContext`.
- `value building`: AST or feature assignment work performed after recognition
  succeeds.

## Public model

- `parse(expr, ctx)` is the semantic entrypoint for parse, recover, and expect.
- `probe(expr, ParseContext&)` is the semantic entrypoint for strict lookahead.
- Expressions may implement `probe_impl(ParseContext&)` for a custom pure probe.
- If no `probe_impl(...)` exists, `probe(...)` falls back to a checkpointed
  strict parse and restores the original state.

## Internal model

- `fast_probe` exists only to speed up hot loops.
- Callers that need correctness semantics must use `probe(...)`, not
  `fast_probe`.
- Semantic parser code must call `probe(...)` for lookahead; `fast_probe` is
  reserved for benchmarked hot loops only.
- Any new `fast_probe` use must be introduced alongside a dedicated microbench
  or an existing benchmark that covers the same hot path.
- `ExpectContext` is modeled as a frontier exploration engine, not as a simple
  `ParseContext` with extra fields. Its responsibility is to evaluate reachable
  parse branches and merge their frontier state.

## Perf guardrails

- Structural refactors must keep `tools/check_parser_perf.sh` green before they
  are considered valid.
- Until a dedicated benchmark exists, do not refactor `Repetition`,
  `ExpectFrontier`, `ParseExpression::Wrapper`, or the infix operator matching
  loop for readability alone.
- The strict hot paths that require extra care are:
  - `ParseContext::leaf(...)`
  - `ctx.skip()`
  - `attempt_parse_strict(...)`
  - infix operator matching and probe fast paths
  - separator-driven grouping handled through repetition
- Any new helper inserted in those paths needs either a benchmark already
  covering it or a new microbenchmark added in the same change.

## Design rule

Keep these concerns separate:

- recognition: deciding whether an expression matches
- lookahead: `probe(...)` and internal `fast_probe`
- repair: recovery heuristics and edit policies
- value building: AST construction, feature assignment, and typed extraction
