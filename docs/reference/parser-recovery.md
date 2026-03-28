# Parser Recovery

This page documents parser-side syntax recovery as it exists today in Pegium.
It describes the real runtime and its current heuristics, not an idealized
architecture.

## Scope

This document covers:

- strict failure analysis
- recovery-window construction and replay
- global attempt classification and ranking
- local recovery heuristics inside parser expressions
- parser diagnostics emitted from recovery edits

It does not cover semantic validation, linking, or language-specific
post-processing.

## Core Invariants

The current recovery runtime is built around these rules:

- strict parsing remains the nominal fast path
- recovery stays grammar-driven and generic
- later recovery may extend a repaired suffix, but it must not weaken an
  already accepted repaired prefix
- local recovery prefers preserving the strongest visible prefix before the
  first edit
- recovery decisions must be replayable from explicit edits and checkpoints
- nominal parsing must not pay runtime overhead for recovery bookkeeping

## Global Pipeline

Recovery runs in this order:

1. Run one strict parse.
2. Capture a strict failure snapshot near the real structural failure.
3. Build one recovery window.
4. Replay the entry rule in recovery mode inside that window.
5. Classify and rank the result.
6. If needed, widen the window and try again.

The main runtime lives in:

- `RecoveryAnalysis.*`
- `RecoverySearch.*`
- `ParseContext.hpp`
- `PegiumParser.cpp`

## Strict Failure Analysis

Strict failure analysis keeps more than a single committed parse length. The
failure snapshot is built from:

- committed strict `parsedLength`
- furthest explored strict cursor
- last visible strict cursor
- visible failure-leaf history around the furthest failure

The first recovery window is anchored from that snapshot so replay starts near
the structural failure rather than only near the last fully committed top-level
node.

## Recovery Windows

A recovery window is the global editing envelope for one recovery attempt.

Each window carries:

- `beginOffset`
- `editFloorOffset`
- `maxCursorOffset`
- `tokenCount`
- `forwardTokenCount`
- `stablePrefixOffset`
- `hasStablePrefix`

### Meaning

- `beginOffset`
  Recovery is inactive before this offset.
- `editFloorOffset`
  Edits are forbidden before this offset.
- `maxCursorOffset`
  Forward-stability bookkeeping starts only after replay reaches the strict
  frontier associated with the window.
- `forwardTokenCount`
  Once replay reaches that frontier, it must continue through a bounded number
  of visible leaves before the window is considered stable.
- `stablePrefixOffset` and `hasStablePrefix`
  These store whether the window preserves a known good committed prefix before
  the first recovery edit.

### Widening

If no acceptable attempt is found, Pegium widens the backward token history of
the current window. One final full-history attempt may still be tried after the
configured search limit is reached.

### Replay Contract

Once a window is accepted, later attempts must replay it first with the same
contract that originally validated the kept edits. That replay contract
preserves at least:

- `beginOffset`
- `editFloorOffset`
- `maxCursorOffset`
- the effective replay `forwardTokenCount`
- the stable-prefix contract

This is what prevents a later distant failure from retroactively degrading an
earlier accepted local repair.

## RecoveryContext

Each recovery attempt creates one `RecoveryContext` and replays one or more
windows inside it.

### Two Cursor Frontiers

The runtime distinguishes two recovery frontiers:

- `furthestExploredCursor`
  Monotone frontier for the whole attempt. It accumulates how far competing
  probes managed to explore.
- `localReplayMaxCursor`
  Frontier of the current local replay sandbox. It is restored by
  `rewind()`, so local candidate evaluation stays idempotent.

This split matters because some probes are pure local sandboxes while others
are intentionally accumulative.

### Recovery State Groups

`RecoveryContext` currently carries three internal state groups:

- `WindowReplayState`
  active window index, forward-token bookkeeping, strict-stability tracking
- `EditBudgetState`
  edit cost, edit count, consecutive deletes, overflow allowance
- `DeleteBridgeState`
  pending hidden trivia that may still be absorbed into the current delete run

Recovery checkpoints restore this state without changing the nominal parse
context.

### Edit Admission

The hard edit budget is evaluated against the active window's own delta, not
against the total cost of replayed earlier windows. This means:

- replaying already accepted windows does not spend the whole budget of a later
  window
- one active window still cannot grow without bound

Default budgets remain:

- `maxRecoveryEditsPerAttempt = 10`
- `maxRecoveryEditCost = 64`

Some narrow delete-only strategies may temporarily overflow those limits, but
global credibility is still judged afterward.

### Forward Stability

For an active window, the effective forward budget is the maximum of:

- the window's original `forwardTokenCount`
- any replay forward requirement captured by later successful edits inside that
  same window

This keeps an actually progressing local repair alive instead of freezing the
window on the tail of the first failure only.

## Global Attempt Classification

Every attempt is classified as one of:

- `StrictFailure`
- `RecoveredButNotCredible`
- `Credible`
- `Stable`

### Meaning

- `StrictFailure`
  the entry rule did not match
- `RecoveredButNotCredible`
  the parse used edits or completed recovery windows, but did not reach the
  credibility bar
- `Credible`
  the parse reached the recovery target
- `Stable`
  the parse full-matched, or replay reached strict stability after the window

### Stable-Prefix Guard

Expensive attempts are downgraded when:

- `editCost > maxRecoveryEditCost`
- the first edit does not preserve the window's stable prefix
- and the attempt is not one of the narrow local-gap insertion cases that still
  produce a credible continuation

This keeps broad expensive repairs from beating a stronger preserved prefix just
because they reach farther.

## Global Ranking

Recovery attempts are projected to a shared `NormalizedRecoveryOrderKey`.
Pegium does not force every local and global candidate into one monolithic
runtime type. Instead, each layer projects onto the same axis vocabulary:

- `prefix`
- `continuation`
- `safety`
- `edits`
- `progress`

For global attempts, the public score still exposes:

- `selection`
  `entryRuleMatched`, `stable`, `credible`, `fullMatch`
- `edits`
  `editCost`, `editSpan`, `entryCount`, `firstEditOffset`
- `progress`
  `parsedLength`, `maxCursorOffset`

### Global Comparison Order

Global ranking currently prefers:

1. selection axes
2. for non-credible ties only: later first edit, lower cost, smaller span,
   fewer script entries
3. progress axes
4. edit axes

`PegiumParser` adds two top-level policies:

- a full match beats a non-full match
- a later attempt can replace the current winner only if it preserves the
  replay contract of already accepted windows

## Secondary Global Heuristics

Two global heuristics remain intentionally explicit.

### Prefix Delete-Retry

When the current attempt has exactly one recovery window starting at offset `0`,
Pegium may run a delete-only prefix scan and retry the entry rule after each
deleted prefix. This is a narrow recovery search fallback for broken file
prefixes such as conflict markers or garbage at the start of the document.

This scan:

- is delete-only
- may temporarily overflow delete budgets
- still relies on the normal global classifier and ranker afterward

### EOF Tail Cleanup

After a successful repaired prefix, Pegium may still apply one final EOF-only
cleanup:

- `trim_long_visible_tail_to_eof(...)`

If the remaining visible tail is only unrecoverable garbage up to EOF, the
runtime may delete that tail as one final cleanup step. This is not a general
widening strategy.

### Non-Credible Fallback Selection

Some non-credible attempts are still allowed as fallback winners. The runtime
currently permits that only when the attempt still shows a strong structural
shape, such as:

- preserved stable prefix plus visible continuation after the first edit
- delete-only prefix recovery with visible continuation after the first edit
- inserted recovery that still reaches full match or stability

## Terminal Recovery

Terminal recovery is the main lexical layer shared by `Literal` and
`TerminalRule`.

### Lexical Recovery Profile

Each recoverable terminal maps to a `LexicalRecoveryProfile`:

- `WordLikeLiteral`
- `WordLikeFreeForm`
- `Separator`
- `Delimiter`
- `OperatorLike`
- `OpaqueSymbol`

This profile decides which local terminal families are even admissible.

### Trivia Gap Profile

The hidden gap before a terminal is summarized by `TriviaGapProfile`:

- `hiddenCodepointSpan`
- `visibleSourceAfterLocalSkip`

The runtime reasons about local lexical proximity from this profile instead of
using a single undifferentiated hidden-gap boolean.

### Anchor Quality

Terminal candidates carry one of two anchor qualities:

- `DirectVisible`
- `AfterHiddenTrivia`

At equal cost and progress, a stronger visible anchor wins before terminal
family priority is consulted.

### Terminal Families

The terminal layer may consider:

- word-boundary split
- fuzzy replacement
- synthetic insertion
- nearby delete-scan

The admissibility matrix is:

- word-boundary split:
  only `WordLikeLiteral`
- fuzzy replacement:
  `WordLikeLiteral` and multi-codepoint `OperatorLike`
- synthetic insertion:
  separators, delimiters, compact symbolic literals, and restricted compact
  literal cases
- nearby delete-scan:
  only across compact hidden trivia, and only for symbol-like terminals

### Hidden Trivia Inside Delete Runs

Delete recovery may absorb hidden trivia into the same delete edit when the run
stays inside the same coarse lexical class. This keeps long deletes contiguous
without relying on line-based heuristics.

### Entry Probes

Entry probes are stricter than full terminal recovery.

- `Literal`
  may wake on word-boundary split or on a locally credible one-edit lexical
  repair
- `TerminalRule`
  exposes entry evidence only for compact separator or delimiter insertion
  after local skipped trivia

Word-like free-form terminals do not wake optional branches only because some
visible source happens to be present.

### Wrapper Propagation

Entry recoverability is forwarded through wrappers such as `ParserRule` and the
type-erased expression layer. Higher-level combinators therefore see the same
entry evidence even when terminals are wrapped by rules.

## Sequence Recovery in Group

`Group` orchestrates local recovery for sequences.

It follows a simple shape:

1. observe sequence facts
2. enumerate only legal local strategies
3. compare them immediately
4. replay the winner

### Sequence Facts

`Group` computes durable facts once per local failure:

- whether the suffix is fully nullable
- whether the strict suffix already starts at the current cursor
- whether the current offset is still inside the active recovery window
- terminal and trivia facts for the current element when relevant

### Sequence Strategies

Current sequence strategies are:

- `InsertCurrentWithCleanTail`
- `DeleteThenInsertCurrent`
- `ReparseCurrentWithoutDelete`
- `SkipNullable`

`SkipNullable` is a real strategy, not an implicit fallback.

### Main Sequence Rules

- a nullable current may yield to a strict suffix that already matches
- local completion of the current element beats a broader restart when it keeps
  a stronger prefix
- terminal-local permissions are derived once from sequence facts instead of
  being recomputed ad hoc at many call sites

## Iteration Recovery in Repetition

`Repetition` is the main local structural hotspot. It now works from explicit
observation and candidate legality data rather than a flat pile of local
booleans.

### Iteration Pipeline

For one iteration, the flow is:

1. build `IterationObservation`
2. derive `IterationCandidateSet`
3. evaluate legal iteration families
4. compare them
5. replay the winner

### Iteration Families

Current iteration families are:

- `NoInsert`
- `InsertRetry`
- `DeleteRetry`

Priority remains fixed:

1. `NoInsert`
2. `InsertRetry`
3. `DeleteRetry`

### Meaning

- `NoInsert`
  replay the current iteration without opening a fresh insertion strategy
- `InsertRetry`
  local completion of the current iteration, possibly with scoped local
  continuation
- `DeleteRetry`
  local restart of the current iteration after deletion

### Local Continuation

Scoped local continuation is reserved for narrowly bounded situations. It is
not a general relaxation of insertion. This keeps separator completion local
instead of turning into broad permissiveness.

### Zero-Min Repetitions

`optional`, `*`, and `repeat<0, N>` share the same zero-min recovery path.

Iterations are still classified internally as:

- `Strong`
- `Weak`

A weak iteration is a repair that technically matches but does not continue
strongly enough after its first edit to be committed greedily.

For zero-min repetitions, Pegium keeps a provisional weak run instead of
committing each weak iteration immediately. That lets the repetition compare:

- keeping the weak run
- restarting from the beginning of that run with a broader delete-only restart

This is what prevents long runs of tiny low-value repairs from being committed
before a better larger restart is considered.

## Branch Recovery in OrderedChoice

`OrderedChoice` reasons in two layers:

1. best local attempt for one branch
2. best branch overall

### Branch Families

The branch-local families are:

- `Strict`
- `StrictDeferred`
- `Editable`
- `EditableDeleteRetry`

### Meaning

- `Strict`
  direct branch success without edits
- `StrictDeferred`
  delayed strict fallback when a branch is structurally promising but not yet
  the preferred editable winner
- `Editable`
  ordinary local repair inside the branch
- `EditableDeleteRetry`
  branch-local restart after delete-only scanning

### Branch Pipeline

For one branch, the runtime currently:

1. evaluates the strict attempt
2. evaluates the best editable attempt
3. optionally evaluates delete-retry
4. keeps the best local branch attempt
5. compares that branch winner to the other branches

### Important Branch Rule

A branch-local delete-retry must not beat a credible local completion only
because it leaves a cleaner cursor shape. Branch comparison still prefers real
continuation after the first edit over cosmetic restart wins.

## Diagnostics

Recovery is exposed publicly through parse diagnostics and recovered CST nodes.

The edit kinds are:

- `Inserted`
- `Deleted`
- `Replaced`
- `Incomplete`

Before they leave the parser, recovery edits are normalized so contiguous runs
of compatible deletes appear as coherent diagnostics instead of fragmented
low-level operations.

## Reading Guide

For code reading, the most useful order is:

1. `RecoveryAnalysis.*`
2. `RecoverySearch.*`
3. `ParseContext.hpp`
4. `RecoveryCandidate.hpp`
5. `TerminalRecoverySupport.hpp`
6. `Group.hpp`
7. `Repetition.hpp`
8. `OrderedChoice.hpp`
