# AGENTS.md

Pegium is a generic C++20 language-engineering toolkit. These are the standing
expectations for working in this repository.

## Build & test

- Build with `cmake --build build -j32`. Never prefix `cmake --build` with a
  timeout.
- The build is slow: for a multi-step change, batch everything and build once.
- Never run `ctest` while a build is still running — wait for the active build
  to finish successfully first.
- Run the tests with `ctest`, and wrap `ctest` / `gtest` / fuzzer runs in a
  shell `timeout`.
- Only build and test when it is relevant to the change.
- Benchmark hot-path changes with `PegiumBench` under callgrind, not just unit
  tests.

## Code style & API

- Use C++20 features wherever they help.
- Match the surrounding code: comment density, naming, and idioms.
- When an invariant says a value must exist, take it by `const&`, not a nullable
  pointer; keep pointers only for genuinely optional data.
- Never add a `nullptr` check for a pointer that is never null by design.
- `const_cast` is forbidden.
- No `thread_local`.
- Use `void*` for type erasure.
- No wrappers that only add one more level of indirection / a trampoline.
- No IIFE unless it genuinely improves readability.
- Reduce the public API surface when a helper can be internalized without
  hurting clarity.
- Document public APIs: short but useful.
- `noexcept` must be honest — do not mark `noexcept` anything that can allocate
  or call a throwing virtual.

## Generic engine & performance contracts

- Pegium is 100% generic. Nothing in the parser or recovery may be hardcoded to
  a specific language's grammar, and optimizations must stay language-agnostic.
- The nominal parse path must carry zero overhead from error recovery.
- The strict-mode parser combinator fast path is off-limits — do not refactor or
  restyle it.
- Thread-safety is a hard contract: the Reference / scope / index /
  document-state read APIs must be safe for concurrent reads without locking.
- There is intentionally no `Scope` wrapper abstraction; the visitor /
  `getScopeEntry` design is deliberate and far faster — do not propose one.

## Working approach

- On any error, investigate to find the root cause instead of patching a
  symptom.
- Keep solutions proportionate to the problem — do not over-engineer.
- When relevant, check how Langium does it (`https://github.com/eclipse-langium/langium`) to
  avoid reinventing or diverging (the main README is the only exception).
