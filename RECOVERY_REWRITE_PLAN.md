# Recovery Rewrite Plan

## Purpose

Converge Pegium's current recovery engine toward a smaller and clearer runtime
built around one shared model.

The first rewrite pass has already landed large parts of that target runtime.
The remaining work is not a fresh rewrite from scratch. It is an architectural
tightening pass: update the plan to reflect what is already true in code, make
the remaining deviations explicit, and close them without relaxing the target
model.

This rewrite is acceptable only if it satisfies all three conditions:

- it genuinely simplifies the recovery model
- it preserves the right invariants
- it does not keep the old complexity alive under cleaner names

The target shape is:

- global pipeline: `strict parse -> snapshot -> plan windows -> replay -> compare`
- local pipeline: `observe -> enumerate -> normalize -> compare -> replay`

## Execution Mode

This rewrite starts directly from Phase 1 on the current codebase.

That choice is pragmatic, but risky. Because of it, every legacy portion touched
by a phase must satisfy one of these conditions before the phase is considered
done:

- it projects cleanly into the new model
- it is removed

No touched legacy block is allowed to survive as an accidental compatibility
layer.

## Non-Negotiable Constraints

- the nominal strict parse path must not pay a recovery cost
- recovery must remain fully generic and grammar-driven
- no heuristic may depend on a specific language grammar
- no heuristic may depend on line structure
- no `thread_local`
- no `const_cast`
- no trampoline wrappers
- no helper may carry policy that escapes the shared recovery pipeline

Public parser-facing types stay stable:

- `ParseOptions`
- `ParseDiagnostic`
- `ParseResult`
- `PegiumParser::parse`

Hard decisions for this rewrite:

- no dual-run
- no shadow mode
- no compatibility layer
- no feature flag
- invariant tests are the oracle
- corpus and probe baselines may change if the new engine is simpler and more
  coherent

## Current Status

This document remains the normative source of truth.

The implementation is stable and close to the target runtime, but the rewrite
is not fully closed yet. The remaining deviations are now small, explicit, and
tracked here instead of being hidden behind a premature "landed everywhere"
claim.

| Phase | Status | What is already true in code | Remaining deviations | What must still be deleted |
| --- | --- | --- | --- | --- |
| 1 | tightening | global pipeline is centered on `StrictFailureEngine`, `WindowPlanner`, `RecoveryContext`, and `run_recovery_search(...)`, non-credible fallback admission goes through one replay-contract family over `resumeFloor + preserved edit prefix`, narrowed-window retries share one overreach predicate and one result-merge contract, the top-level narrowed rerun is iterative instead of recursive, and window acceptance is split into pure qualification plus replay application so the main loop, validation-only continuation probes, and trimmed-full-match validation probes share one explicit `selectable / fallback / none` decision without a second acceptance enum/switch layer; window candidate handling no longer routes through a separate raw candidate-kind enum/classifier ahead of that qualification step, trimmed-tail follow-up candidates are requalified directly inside the trimmed-tail validation path, with no separate outer requalification pass, no dedicated validated-follow-up helper, and no standalone trimmed-tail validation helper, trimmed-tail validation now returns its enriched same-window result directly instead of copying only qualified winners out of a follow-up result, follow-up candidates stay in that same window result even when they are only ranked inputs for later narrowing instead of already-qualified winners, the validation path now merges follow-up candidates directly into the local `WindowRecoverySearchResult` instead of returning a secondary follow-up result object, trimmed-tail validation no longer re-enters a nested `evaluate_recovery_window_attempts(...)` call for the follow-up window, and the untrimmed validation attempt now always re-enters the normal same-window qualification flow even when the window budget no longer permits planning an extra trimmed-tail follow-up window | trimmed-tail validation and follow-up generation still form a smaller special path around the main window flow, and fallback/narrowing orchestration is still richer than the final target contract | any residual follow-up or fallback helper that carries acceptance policy instead of pure candidate generation |
| 2 | landed | terminal recovery is normalized around lexical/trivia facts and shared terminal candidates, and global search no longer depends on line-structure heuristics | no open deviation remains outside the tightening targets listed below | none |
| 3 | tightening | `Group` enumerates structural attempts, computes terminal legality once per failure site, routes current-failure and tail-failure winner selection through explicit selection steps before replay, no longer keeps the `preferStructuredVisibleInsert` bypass ahead of the shared comparator, threads current-failure and tail-failure through explicit facts plus legality/replay inputs instead of one bundled `SequenceRecoveryPlan`, now passes terminal legality through explicit booleans instead of a `TerminalSequenceLegality` mini-type, reuses the shared `choice` candidate comparator directly instead of a dedicated boundary-preference wrapper, no longer keeps separate single-attempt replay bypasses for current-failure or tail-failure recovery, now computes current-failure `reparse current without delete` legality once at the failure site instead of recalculating it inside selection, now carries structural winners through explicit replay plans instead of a `SequenceRecoveryAttemptKind` enum plus late replay switch, with no surviving `SequenceRecoveryAttempt` carrier around winner selection, no longer routes replay-plan construction through trivial local factory helpers, and no longer duplicates the winner insert-tail replay body internally inside `try_insert_missing_element(...)` | `try_insert_missing_element(...)` still forms a local mini-family richer than the minimal target shape | any helper or bundle that mixes observation, legality, and replay decisions back together |
| 4 | tightening | `Repetition` works through iteration observation, candidate enumeration, replay plans, and a structural comparator projected through the shared key, retry-attempt selection plus boundary-unsafe rejection flow through one explicit winner-selection step, boundary safety is expressed as explicit protection facts instead of overlapping boolean triplets at the selection site, the selected winner now carries its replay plan directly instead of routing through a separate `SelectedIterationRetry` wrapper plus late winner-plan dispatch helper, the legal retry plans now flow through the selection helpers directly instead of a bundled `IterationCandidateSet`, boundary protection is now consumed through one shared retry-legality evaluation step instead of being re-applied ad hoc at each retry branch, retry plans now carry their own boundary-protection facts instead of having selection rebuild them as a parallel local side-channel, retry evaluation now applies that boundary-unsafe legality directly while materializing each retry attempt instead of routing through a separate legal-retry wrapper plus post-hoc discard helper, the same-start boundary-literal insert invariant is applied through the shared structural candidate comparator instead of a local `Repetition` dominance branch, the boundary-preserving `allowDelete=false` insert-retry variant is now enumerated with the other legal retry plans and compared as a normal retry candidate instead of being injected later inside winner selection, the deep-started-optional delete-retry veto now lives directly in retry-plan legality instead of post-adjusting an already enumerated plan, and `try_recovery_iteration(...)` no longer commits a lone legal retry through a separate single-retry bypass before the shared winner-selection flow | retry legality and boundary protection are not yet fully collapsed into the shared model | any remaining retry dominance helper whose effect can be expressed as legality plus shared comparator axes |
| 5 | tightening | `OrderedChoice` derives no-edit attempts first, no longer keeps entry-start probing in a whole-choice cache or bundled `ChoiceBranchFacts` mini-model, routes replay through the shared choice comparator plus explicit boundary-preservation invariants, no longer needs a dedicated `ChoiceReplayRecoveryCandidate` wrapper, calls the shared clean-boundary invariant directly instead of rewrapping it in local helpers, evaluates `no strict start signal` legality through an explicit filtering predicate over the enumerated attempts instead of storing candidate-local veto flags or applying a post-selection correction, now applies that legality at branch-candidate construction time instead of as a second local filter after candidate construction in the selection loop, no longer keeps separate helper functions just to restate that branch-local legality outside the candidate-construction site, now defers entry-start probes until after no-edit observation so clean-boundary fast paths do not pay for unnecessary branch-entry probing, runs strict-start and no-strict-start selection through one branch-enumeration loop and one candidate-construction path instead of separate local branch families, consumes entry-start probing branch-by-branch directly in that selection loop instead of staging it in a whole-choice cache or precomputed probe table, now performs that branch-by-branch probing only when the candidate actually needs it for restart admission or no-strict-start legality instead of plumbing a per-branch signal through selection, and no longer routes those branch-local probes through a dedicated helper that just replays `probe_recoverable_at_entry(...)` with rewind bookkeeping, reuses the same shared `choice` candidate comparator as `Group` instead of keeping a separate replay-candidate wrapper, no longer carries restart replay through a separate `RestartReplayPlan` bundle, and no longer keeps restart delete-scan state in a dedicated local `RestartScanState` sub-model | entry-start probing still lives as local policy instead of a thinner projected shape | any post-selection or branch-local legality layer that cannot be explained as facts plus legal candidate filtering |
| 6 | landed | diagnostics are produced from normalized edit scripts and the final parser-facing projection is script-first | no open deviation remains | none |
| 7 | tightening | large legacy blocks are removed, structural ranking is projected through normalized order keys, the specialized candidate families route through one explicit `NormalizedRecoveryOrderKey` comparison entry, repeated shared comparator axes such as `matched / editCost / editCount` are factored once instead of being open-coded in every profile, repeated bidirectional axis decisions in `choice`/`attempt` comparators now share one helper instead of open-coding `lhs/rhs` checks at each block, the profile-specific key-only comparator wrappers have been deleted in favor of that single entry, terminal/editable/progress/structural/attempt call sites invoke the shared entry directly, `choice` candidate ordering now goes through one shared candidate comparator with explicit clean-boundary / preferred-boundary constraints instead of two candidate-specific wrappers or extra preferred-boundary bypass helpers, those `choice` boundary invariants now live in named predicates instead of an undifferentiated comparator block, the distinct `choice` profile is now factored into explicit base axes, same-start continuation preference, same-start rewrite preference, and final generic axes instead of one monolithic comparator block, and those comparison stages now live in named helpers instead of anonymous local lambdas, and the structural same-start boundary-literal insert invariant is now owned by the shared structural comparator rather than by `Repetition` | the comparison family still keeps a distinct `choice` profile and other profile-specific ordering blocks richer than the final "one thin projection" target | any comparator profile that still owns non-invariant policy instead of only axis projection |

## Cross-Phase Deviations

Open deviations remain and are intentionally tracked here until they are
deleted or projected cleanly into the shared invariant list below.

- `RecoverySearch.cpp`: trimmed-tail follow-up generation and fallback/narrowing
  orchestration are still more specialized than the target Phase 1 contract.
  The external requalification pass, the dedicated validated-follow-up helper,
  the standalone trimmed-tail validation helper, and the nested follow-up
  `evaluate_recovery_window_attempts(...)` call are gone; ranked follow-up
  candidates now stay in the same window result instead of being dropped
  before narrowing, trimmed-tail validation returns that enriched window
  result directly instead of copying only qualified winners out of the
  follow-up search, and follow-up candidates are merged directly into the
  local window result instead of living in a secondary returned search result,
  the untrimmed validation attempt now always enters the same window-level
  qualification flow even when no extra follow-up window can be planned, but
  trimmed-tail validation and follow-up generation still form a smaller
  secondary orchestration layer inside the main window flow.
- `RecoveryCandidate.hpp`: the comparison family is still partly fragmented,
  especially around the distinct `choice` profile, even though `choice`
  candidate ordering no longer splits into separate replay-vs-boundary
  wrappers or standalone same-start preferred-boundary helpers.
- `Group.hpp`: structural attempt handling is still richer than the minimal
  `facts -> legal candidates -> compare -> replay` skeleton, especially
  around `try_insert_missing_element(...)`, even though winner replay now goes
  through explicit replay plans instead of a local kind enum plus replay
  switch, no `SequenceRecoveryAttempt` carrier remains around winner
  selection, and `try_insert_missing_element(...)` no longer duplicates its
  winning insert-tail replay body between candidate evaluation and winner
  replay.
- `OrderedChoice.hpp`: branch entry-start probing is still local policy
  instead of a thinner projection, even though it is now consumed directly in
  the branch-selection loop without a whole-choice cache, whole-choice signal,
  or staged per-choice probe table, and is only triggered when the candidate
  actually needs it for restart admission or no-strict-start legality.
- `Repetition.hpp`: retry/boundary policy still keeps local retry-legality and
  boundary-protection machinery even though retry boundary protection now
  flows through one shared retry-legality step and is carried by the retry
  plans themselves, and the boundary-preserving `allowDelete=false`
  insert-retry variant is now enumerated with the other legal retry plans
  before it competes as a normal candidate instead of being injected inside
  selection.
- delete-scan overflow-budget handling now shares one generic scope in
  `RecoveryUtils.hpp`, but the higher-level delete-scan generation paths are
  still distributed across `TerminalRecoverySupport.hpp`,
  `OrderedChoice.hpp`, `Repetition.hpp`, and `RecoverySearch.cpp`.
- `RecoveryScore` / `EditTrace` still exist as an intermediate projection
  before the normalized key and debug surfaces, which is lower priority than
  the runtime-policy cleanup above but remains a valid residual tightening
  target once the comparator family is flatter.

## Shared Invariants Still Present

The rewrite intentionally keeps a small set of explicit shared invariants.

Only the rules in this section are acceptable long-term survivors. Any other
special case found in the runtime should be treated as open deviation and either
projected here with a named invariant or removed.

- preferred same-start boundary edits keep the smallest rewrite before a farther continuation can win
- a clean no-edit choice replay preserves an already visible suffix boundary unless the competing replay starts strictly after it
- non-credible fallback extension may only advance by preserving the already committed replay prefix and then making later progress
- a full match obtained only by trimming a visible tail must be revalidated without the trim before it can replace the untrimmed continuation

## Architectural Rules

### Primary rule

Preserve invariants, not historical strategy names.

### Simplicity rule

Every new abstraction must remove decision-making complexity. A phase is not
accepted if it only relocates complexity.

### Shared-order rule

No combinator may own:

- a private comparator
- a private score
- a private strategy family that cannot project onto the shared model

### Facts rule

Observed facts are descriptions only.

Facts must:

- be cheap to compute
- be locally verifiable
- not encode ranking
- not encode partial decisions

If a fact reads like a partial winner, it is not a fact.

### Helper rule

No helper may hide policy outside the shared pipeline:

- observe
- enumerate
- normalize
- compare
- replay

### Vocabulary rule

Do not reintroduce legacy family names when the normalized primitive model is
enough.

### Cleanup rule

Each phase must delete real legacy logic, not just wrap it.

### Explainability rule

The final engine must be easier to explain than the current one.

### Documentation rule

`RECOVERY_REWRITE_PLAN.md` is the only normative phase plan.

The `tools/recovery/` directory contains exploratory design notes and annexes.
Those notes may help explain or prototype a direction, but they do not define
the rewrite contract. If any note in `tools/recovery/` conflicts with this
document, this document wins.

## Target Runtime

### 1. StrictFailureEngine

Responsibility:

- run strict parse
- produce the minimal recovery snapshot

It keeps only the information needed to start recovery:

- committed `parsedLength`
- `lastVisibleCursorOffset`
- `furthestStrictCursorOffset`
- minimal visible failure context

It does not:

- rank attempts
- widen windows
- run local heuristics

### 2. WindowPlanner

Responsibility:

- build recovery windows
- widen them deterministically
- carry all inter-attempt memory

Each accepted window defines the replay contract for later attempts.

No other subsystem may keep hidden inter-attempt memory.

### 3. RecoveryContext

Responsibility:

- hold replay-time state only
- expose checkpoints and edit primitives

Internal state is limited to:

- `WindowReplayState`
- `EditBudgetState`
- `DeleteBridgeState`

Mandatory cursor split:

- `furthestExploredCursor`
- `localReplayMaxCursor`

Rules:

- `furthestExploredCursor` is monotone within one attempt
- `localReplayMaxCursor` is restored by `rewind()`
- checkpoints restore replay state only
- if an internal state fragment cannot be tied to a replay invariant, it must
  not exist

### 4. Local Recovery Engine

All local recovery follows the same protocol:

1. observe facts
2. enumerate legal candidates
3. normalize candidates
4. compare candidates
5. replay the winner

This applies to:

- terminals
- sequences
- repetitions
- choices

### 5. RecoveryComparator

All normalized candidates compare through one lexicographic key.

No floating score.
No partial order.
No local ranking exceptions.

## Shared Model

### RecoveryOrderKey

All candidate shapes project to the same ordered axes:

- `prefix`
- `continuation`
- `safety`
- `edits`
- `progress`

Each axis exists only to protect an invariant or to remove a special rule.

If an axis only preserves a historical preference, it must be removed.

### RecoveryOrderKey review

At the end of every phase:

- no new axis is allowed unless it replaces an older special rule
- no existing axis survives without a named invariant or a simplification
  benefit
- overlapping sub-axes must be reduced aggressively

The most suspect areas to keep under active review are:

- `continuationStrength`
- `strictSuffixReachable`
- `visibleProgressAfterFirstEdit`
- `localityRank`

### Candidate shapes

Use only these implementation shapes:

- `TerminalCandidate`
- `StructuralCandidate`
- `GlobalAttemptCandidate`

These shapes are implementation conveniences, not conceptual subsystems.

Rules:

- they all use the same comparison key
- they all follow the same replay protocol structure
- no ranking-only field may exist in one shape without a clear semantic
  equivalent in the others
- if specialization grows, reduce the shape count instead of extending it

### Edit primitives

Normalize recovery to four primitives:

- `Keep`
- `Insert`
- `Delete`
- `Replace`

Interpretation rules:

- `Restart` is `Delete` plus structural replay
- `DeleteThenInsert` becomes `Replace`
- `SkipNullable` may survive only while it explains a structural invariant that
  plain `Keep` would hide
- `StrictDeferred` may survive only as a temporary legality label, never as a
  first-class strategy family

### Residual-debt watchlist

The main residual concepts to watch are:

- `SkipNullable`
- `WeakRunState`

At the end of each phase, every surviving residual concept must state:

- which invariant it still explains
- why the reduced primitive model does not yet express that invariant clearly
- by which later phase it is expected to disappear or be re-justified

## Rewrite Phases

### Phase 1. Replace the Global Orchestration

Build the new global pipeline on the current codebase:

- `StrictFailureEngine`
- `WindowPlanner`
- `RecoveryContext`
- `runRecoverySearch(...)`
- shared global comparison on `RecoveryOrderKey`

`PegiumParser` must stop being the place where global recovery policy lives.

Phase 1 must not hard-code assumptions about legacy local strategy families.

Exit criteria:

- global recovery control flow lives in the new pipeline
- `PegiumParser` is reduced to strict parse entry and result assembly
- touched global legacy logic is either projected cleanly or removed
- no new helper added in this phase hides policy

### Phase 2. Rewrite Terminal Recovery

Rebuild terminal recovery around:

- `LexicalRecoveryProfile`
- `TriviaGapProfile`

Terminal recovery is allowed to enumerate only normalized terminal candidates
built from:

- `Keep`
- `Insert`
- `Replace`
- `DeleteScan`

Terminal heuristics must derive from shared lexical and trivia facts only.

Exit criteria:

- no line-based terminal heuristic remains
- `Literal` and `TerminalRule` no longer diverge on admissibility logic
- no terminal ranking logic exists outside the shared order model
- touched terminal legacy logic is either projected cleanly or removed

### Phase 3. Rewrite Group

Turn `Group` into a pure orchestrator.

`Group` observes facts, enumerates legal structural candidates, normalizes them,
compares them, and replays the winner.

Allowed structural moves:

- keep current
- repair current
- restart current
- skip nullable

Exit criteria:

- `Group` owns no private ranking
- `Group` has explicit blocks for facts, candidate legality, comparison, and replay
- touched pairwise or ranking-only helpers in `Group` are deleted rather than wrapped
- `Group` no longer re-derives permissions ad hoc during replay
- touched group legacy logic is either projected cleanly or removed

### Phase 4. Rewrite Repetition

Rebuild `Repetition` as a continuation automaton.

Core local notions:

- iteration observation
- iteration legality
- normalized iteration candidates
- bounded weak-run handling

`optional`, `*`, and `repeat<0, N>` must share the same recovery model.

Exit criteria:

- no greedy weak-iteration commit remains
- no repetition-private comparator remains
- `Repetition` has explicit blocks for facts, candidate legality, comparison, and replay
- touched pairwise or ranking-only helpers in `Repetition` are deleted rather than wrapped
- the only surviving special concept is a tightly bounded residual concept with
  an explicit invariant
- touched repetition legacy logic is either projected cleanly or removed

### Phase 5. Rewrite OrderedChoice

Rebuild choice recovery as:

- observe branch facts
- produce the best normalized candidate per branch
- compare branches through the shared order
- replay the winning branch

Exit criteria:

- no branch-private ranking remains
- touched pairwise or ranking-only helpers in `OrderedChoice` are deleted rather than wrapped
- `OrderedChoice` has explicit blocks for facts, candidate legality, comparison, and replay
- no branch-local strategy family survives outside the normalized model
- restart is expressed through normalized delete-plus-replay behavior
- touched choice legacy logic is either projected cleanly or removed

### Phase 6. Normalize Edits and Diagnostics

Rebuild edit and diagnostic production from normalized edit scripts.

The output must come from a canonical edit representation, not from legacy
strategy shapes.

Exit criteria:

- diagnostics derive from normalized edits
- contiguous deletes are fused canonically
- compatible inserts are fused canonically
- no diagnostic structure depends on legacy strategy families

### Phase 7. Reduce Residual Complexity

This phase is mandatory even if the engine already passes the tests.

Its goal is to remove what survived only as transitional debt:

- shrink `RecoveryOrderKey` if any axis is still only preserving historical
  preference
- remove labels that no longer explain a distinct invariant
- collapse candidate-shape differences if they have started to specialize
- delete helpers that add only indirection
- rename vocabulary that still reflects the legacy model instead of the
  normalized one

Exit criteria:

- the model is simpler to explain than the old one
- no remaining abstraction survives without a clear invariant
- no surviving helper exists only to preserve a legacy decomposition
- no pairwise special rule survives unless it has become a named shared invariant

## Per-Phase Review Checklist

Every phase ends with this review:

### 1. Order-key review

- which axes are still needed
- which sub-axes overlap
- whether any new axis replaced a real special rule

### 2. Survivor review

- which labels remain
- which residual concepts remain
- which invariant each one explains
- by which phase each one should disappear or be re-justified

### 3. Legacy cleanup review

- which legacy code was removed
- which touched legacy code still remains
- why the remaining portion is still aligned with the new model

### 4. Helper review

- which helpers were introduced
- whether any helper now hides policy

### 5. Explainability review

For at least one representative failure, the engine must be able to explain:

- observed facts
- enumerated candidates
- normalized keys
- winner
- rejection reasons

## Testing Policy

The invariant suite is the oracle.

Tests are meant to protect:

- public parser contracts
- replay and window invariants
- structural recovery invariants
- normalized diagnostic behavior

Tests are not meant to protect:

- historical strategy names
- legacy internal rankings
- accidental output shapes caused by the old engine

Corpus and probe baselines may be rewritten, but only after the new model
produces a simpler and more coherent result.

## Definition of Done

The rewrite is complete only if all of the following are true:

- the full test suite is green
- the recovery engine is organized around the shared global and local pipelines
- all local recovery uses the same compare-and-replay model
- no touched legacy logic survives outside clean projection into the new model
- no private comparator or private score remains in a combinator
- the remaining residual concepts are either gone or still justified by a clear
  invariant
- the final model is simpler to explain than the current one
