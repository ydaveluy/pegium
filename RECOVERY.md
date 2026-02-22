# Recovery Algorithm (PEG Parser)

## Purpose

The recovery system is designed to keep parsing useful even when input is
invalid. Its goals are:

- preserve as much structural information as possible,
- continue parsing after local damage,
- emit explicit edit diagnostics,
- avoid pathological runtime on malformed input.

This is character/codepoint-level recovery (no separate lexer token stream).

## High-Level Strategy

Recovery is a staged search:

1. **Strict recognition** (no edits allowed).
2. **Local editable recovery** near the failure frontier.
3. **Global editable recovery** if local recovery is insufficient.

The parser always prefers low-disruption behavior:

- first, prove that strict parsing is enough,
- then allow edits in a bounded area,
- finally expand the edit area only when needed.

## Core State Model

Recovery maintains:

- current cursor (where parsing currently is),
- max cursor reached (failure frontier / progress anchor),
- edit permissions (insert/delete enabled or disabled),
- an editable window `[floor, ceiling]`,
- a delete budget for consecutive codepoint deletions,
- diagnostics emitted by performed edits.

This gives deterministic control over where and how recovery is allowed.

## Edit Operations

Recovery can apply three atomic edits:

- **Insert expected element** (hidden, synthetic node),
- **Delete one codepoint**,
- **Replace a consumed span with expected element**.

Each successful edit emits a diagnostic:

- `Inserted`,
- `Deleted`,
- `Replaced`.

Edits are rejected if they violate:

- strict mode,
- edit window limits,
- delete budget,
- policy constraints (for force-insert behavior).

## Strict vs Editable Modes

### Strict mode

Strict mode forbids edits. It is used:

- as the first recovery stage,
- inside lookahead-style logic,
- as a fast validation path before trying expensive alternatives.

In strict mode, parsing behavior is intended to mirror normal parsing logic as
closely as possible (same branch decisions, same structural progression), just
through the recovery entrypoints.

### Editable mode

Editable mode enables insertion/deletion/replacement, but still under guardrails
(window + budgets + policy).

This prevents global over-correction and keeps recovery localized first.

## Local-Then-Global Recovery

When strict parsing fails:

1. Determine the strict progress frontier (`max cursor reached`).
2. Open a local edit window starting at that frontier.
3. Try editable recovery in that local window.
4. If still failing, retry editable recovery with a wider/global window.

This two-step expansion is critical:

- local recovery keeps performance and AST stability,
- global recovery maximizes eventual parse completion.

## Why Ordered Choice Needs Special Care

Ordered alternatives are the main ambiguity hotspot in PEG recovery.

A robust approach is:

1. run a strict probe first (no edits) to preserve nominal alternative choice,
2. only if strict fails, attempt editable alternatives.

This avoids selecting a wrong branch too early due to aggressive edits.

## Repetition and Group Behavior

For sequence/group-style constructs:

- strict mode should follow plain recognition flow,
- editable mode should avoid unnecessary strict re-probes inside each step,
- loops must guard against zero-progress iterations.

Zero-progress protection is mandatory to avoid infinite loops under damage.

## Predicates (Lookahead) Under Recovery

Lookaheads (`and` / `not`) should behave as pure probes:

- evaluate sub-expression in no-edit mode,
- always rewind probe side effects,
- return only boolean success/failure of the probe.

They must not consume input nor commit edits.

## Recovery Policies

Policy hooks can restrict or allow certain edits (especially forced insertions).
Typical policy design:

- allow structural sync punctuation insertion,
- avoid broad insertion of semantic literals everywhere,
- keep forced edits conservative.

Good policy design has a direct impact on both AST quality and runtime.

## Diagnostics and Recovered Trees

Recovered nodes should be explicitly marked so downstream consumers can:

- distinguish original vs synthetic/edited structure,
- preserve canonical values when needed,
- display meaningful editor diagnostics.

The AST can still be “usable” even when parts of the CST are recovered.

## Performance Principles

Main performance rules:

1. Keep strict path as close as possible to normal parsing behavior.
2. Avoid duplicate strict attempts inside nested combinators.
3. Use local windows before global fallback.
4. Cap consecutive deletions.
5. Prevent repeated no-progress loops.

If malformed input causes major slowdown, first check:

- repeated strict re-probing,
- excessive mark/rewind churn,
- over-large local windows,
- permissive delete budgets,
- choice combinators exploring too many editable branches.

## Tuning Guidelines

Key knobs:

- **Local recovery window size**:
  - smaller = faster, less robust,
  - larger = more robust, potentially slower.
- **Max consecutive deletions**:
  - smaller = safer/faster,
  - larger = more tolerant, can degrade runtime.

Recommended workflow:

1. start conservative,
2. run representative corrupted-input tests,
3. compare throughput on healthy + damaged inputs,
4. increase tolerance only where it improves real recovery outcomes.

## Practical Mental Model

Think of recovery as a constrained edit search:

- strict first,
- local edits next,
- global edits last,
- always bounded by explicit guardrails.

This keeps PEG recovery predictable, performant, and useful for tooling that
needs best-effort ASTs from imperfect source text.
