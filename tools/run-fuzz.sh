#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage: tools/run-fuzz.sh [options]

Run Pegium's FuzzTest targets and persist discovered inputs.

By default the script runs the two Stress fuzz tests; use `--test all` (or a
preset) to enroll the 3 adversarial / workspace-relink tests as well.

Tests (alias → fully-qualified name → host binary):
  pipeline, single                 → PegiumWorkspaceFuzzTest.StressLanguageSingleDocumentPipeline      (workspace)
  build, arbitrary                 → PegiumWorkspaceFuzzTest.StressLanguageArbitraryDocumentBuild      (workspace)
  adv-pipeline, adv-single         → PegiumWorkspaceFuzzTest.AdversarialLanguageSingleDocumentPipeline (workspace)
  adv-build, adv-arbitrary         → PegiumWorkspaceFuzzTest.AdversarialLanguageArbitraryDocumentBuild (workspace)
  relink, workspace                → PegiumWorkspaceFuzzTest.AdversarialWorkspaceRelinkAndRecovery     (workspace)
  chain, increment                 → PegiumWorkspaceFuzzTest.StressLanguageIncrementalEditChain        (workspace)
  arith-kw, arithmetics            → PegiumExampleKeywordFuzzTest.ArithmeticsKeywordFuzz              (examples)
  domain-kw, domainmodel           → PegiumExampleKeywordFuzzTest.DomainModelKeywordFuzz              (examples)
  state-kw, statemachine           → PegiumExampleKeywordFuzzTest.StateMachineKeywordFuzz             (examples)
  req-kw, requirements             → PegiumExampleKeywordFuzzTest.RequirementsKeywordFuzz             (examples)
  all                              → expands to ALL workspace tests above
  all-examples                     → expands to ALL example keyword fuzz tests above
  all-everything                   → expands to BOTH groups

Options:
  --build-dir DIR        Build directory containing PegiumWorkspaceFuzzTest.
                         Default: build-fuzz-ci  (or per --config)
  --config NAME          Pre-set --build-dir from a known config:
                           coverage   → build-fuzz-ci             (default, FuzzTest coverage)
                           asan       → build-asan-clang          (AddressSanitizer)
                           ubsan      → build-ci-sanitizer-check  (UndefinedBehaviorSanitizer)
                         The build directory must already contain the binary.
  --binary PATH          Explicit path to PegiumWorkspaceFuzzTest.
                         Overrides --build-dir / --config.
  --corpus DIR           Root directory used for saved corpus files.
                         Each fuzz test writes into <DIR>/<sanitized-test-name>.
                         Default: <repo-root>/.fuzz/out
  --corpus-database DIR  Experimental FuzzTest/Centipede corpus database path.
                         Not used by default.
  --jobs N               Number of independent fuzz processes per test.
                         The script forks N sibling copies of the binary, each
                         running `--fuzz=X --fuzz_for=T --corpus_database=DB`,
                         all sharing the same corpus_database (FuzzTest's DB
                         is concurrent-safe across processes). This works even
                         when the binary was NOT built with Centipede support
                         (the binary's own `--jobs` flag silently no-ops in
                         that case).
                         A default corpus_database is provisioned at
                         <corpus>/db when N > 1. Pass N=1 to force a single
                         in-process run.
                         When running multiple tests in parallel, each test
                         gets `floor(N / num_tests)` siblings; with
                         --sequential the full budget goes to the active test.
                         Default: detected with nproc/getconf, otherwise 1
  --sequential           Run tests one after another (each one gets the full
                         --jobs budget). Recommended over parallel when running
                         on a busy machine: parallel × jobs explodes the worker
                         count when many tests run together.
  --time DURATION        Active fuzzing budget passed to --fuzz_for.
                         IMPORTANT: this is the time spent fuzzing AFTER seed
                         replay and warm-up. Total wall-clock per test will be
                         seed-replay + (--time × jobs).
                         Default: 10m  (overridden by --preset)
  --stack-limit-kb N     Stack guard passed to FuzzTest --stack_limit_kb.
                         Default: 256. The upstream 128 KiB default is too
                         tight for recovery-heavy coverage builds.
  --test NAME            Test to run. May be passed multiple times. See aliases above.
  --preset NAME          Convenience preset overriding test set, time, jobs:
                           smoke      → all 6 tests, --time 5m
                           deep       → all 6 tests, --time 1h, --continue-after-crash, --summary
                           campaign   → all 6 tests, --time 6h, --continue-after-crash,
                                        --extract-findings, --summary, --sequential
  --continue-after-crash Keep fuzzing after a finding is discovered.
  --print-subprocess-log Forward FuzzTest subprocess logs.
  --extract-findings     After each run, copy any crash inputs from corpus dir
                         into <corpus>/findings/<sanitized-test>/<timestamp>_*.bin
                         and write a one-line .txt summary alongside.
                         Implied by --preset campaign.
  --summary              Print a per-test post-mortem table (status, runs, edges, findings).
                         Implied by --preset deep / campaign.
  --force                Skip the safety check that aborts when another fuzz
                         binary is already running.
  --no-color             Disable ANSI color in summary output.
  --help                 Show this help.

Examples:
  tools/run-fuzz.sh --preset smoke
  tools/run-fuzz.sh --preset deep --jobs 32
  tools/run-fuzz.sh --preset campaign --jobs 32 --config asan
  tools/run-fuzz.sh --test all --time 30m --extract-findings --summary
  tools/run-fuzz.sh --test relink --time 1h --jobs 16

Build the fuzz binary first (if not already built):
  CC=clang CXX=clang++ cmake -S . -B build-fuzz-ci \\
    -DBUILD_TESTING=ON -DPEGIUM_BUILD_EXAMPLES=OFF \\
    -DPEGIUM_BUILD_FUZZ_TARGETS=ON -DFUZZTEST_FUZZING_MODE=ON
  cmake --build build-fuzz-ci --target PegiumWorkspaceFuzzTest -j32
EOF
}

resolve_test_name() {
  case "$1" in
    pipeline|single|StressLanguageSingleDocumentPipeline|PegiumWorkspaceFuzzTest.StressLanguageSingleDocumentPipeline)
      printf '%s\n' "PegiumWorkspaceFuzzTest.StressLanguageSingleDocumentPipeline"
      ;;
    build|arbitrary|StressLanguageArbitraryDocumentBuild|PegiumWorkspaceFuzzTest.StressLanguageArbitraryDocumentBuild)
      printf '%s\n' "PegiumWorkspaceFuzzTest.StressLanguageArbitraryDocumentBuild"
      ;;
    adv-pipeline|adv-single|AdversarialLanguageSingleDocumentPipeline|PegiumWorkspaceFuzzTest.AdversarialLanguageSingleDocumentPipeline)
      printf '%s\n' "PegiumWorkspaceFuzzTest.AdversarialLanguageSingleDocumentPipeline"
      ;;
    adv-build|adv-arbitrary|AdversarialLanguageArbitraryDocumentBuild|PegiumWorkspaceFuzzTest.AdversarialLanguageArbitraryDocumentBuild)
      printf '%s\n' "PegiumWorkspaceFuzzTest.AdversarialLanguageArbitraryDocumentBuild"
      ;;
    relink|workspace|AdversarialWorkspaceRelinkAndRecovery|PegiumWorkspaceFuzzTest.AdversarialWorkspaceRelinkAndRecovery)
      printf '%s\n' "PegiumWorkspaceFuzzTest.AdversarialWorkspaceRelinkAndRecovery"
      ;;
    chain|increment|StressLanguageIncrementalEditChain|PegiumWorkspaceFuzzTest.StressLanguageIncrementalEditChain)
      printf '%s\n' "PegiumWorkspaceFuzzTest.StressLanguageIncrementalEditChain"
      ;;
    arith-kw|arithmetics|ArithmeticsKeywordFuzz|PegiumExampleKeywordFuzzTest.ArithmeticsKeywordFuzz)
      printf '%s\n' "PegiumExampleKeywordFuzzTest.ArithmeticsKeywordFuzz"
      ;;
    domain-kw|domainmodel|DomainModelKeywordFuzz|PegiumExampleKeywordFuzzTest.DomainModelKeywordFuzz)
      printf '%s\n' "PegiumExampleKeywordFuzzTest.DomainModelKeywordFuzz"
      ;;
    state-kw|statemachine|StateMachineKeywordFuzz|PegiumExampleKeywordFuzzTest.StateMachineKeywordFuzz)
      printf '%s\n' "PegiumExampleKeywordFuzzTest.StateMachineKeywordFuzz"
      ;;
    req-kw|requirements|RequirementsKeywordFuzz|PegiumExampleKeywordFuzzTest.RequirementsKeywordFuzz)
      printf '%s\n' "PegiumExampleKeywordFuzzTest.RequirementsKeywordFuzz"
      ;;
    *)
      printf 'Unsupported fuzz test: %s\n' "$1" >&2
      return 1
      ;;
  esac
}

# Map a fully-qualified test name to its host binary basename. Lets us
# dispatch to the right executable when running mixed test sets.
binary_for_test() {
  local test_name="$1"
  case "$test_name" in
    PegiumWorkspaceFuzzTest.*)        printf 'PegiumWorkspaceFuzzTest\n' ;;
    PegiumExampleKeywordFuzzTest.*)   printf 'PegiumExampleKeywordFuzzTest\n' ;;
    *)
      printf 'Unknown host binary for test: %s\n' "$test_name" >&2
      return 1
      ;;
  esac
}

resolve_build_dir_for_config() {
  case "$1" in
    coverage|fuzz|fuzz-ci|"") printf '%s\n' "build-fuzz-ci" ;;
    asan|address)             printf '%s\n' "build-asan-clang" ;;
    ubsan|undefined)          printf '%s\n' "build-ci-sanitizer-check" ;;
    *)
      printf 'Unsupported config: %s (expected coverage|asan|ubsan)\n' "$1" >&2
      return 1
      ;;
  esac
}

# Per-binary corpus_database path. Different binaries have disjoint coverage
# maps and registered tests; sharing a single database between binaries
# triggers spurious "test not found" / db-layout assertions.
corpus_database_for_binary() {
  local root="$1"
  local binary_name="$2"
  printf '%s/%s\n' "$root" "$binary_name"
}

sanitize_file_name() {
  printf '%s\n' "$1" | tr '/:' '__' | tr '.' '_'
}

detect_default_jobs() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
    return
  fi
  if command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN
    return
  fi
  printf '1\n'
}

detect_binary_path() {
  local build_dir="$1"
  local candidate="$build_dir/tests/fuzz/PegiumWorkspaceFuzzTest"
  if [[ -x "$candidate" ]]; then
    printf '%s\n' "$candidate"
    return
  fi

  local found
  found="$(find "$build_dir" -type f -name PegiumWorkspaceFuzzTest -print -quit 2>/dev/null || true)"
  if [[ -n "$found" ]]; then
    printf '%s\n' "$found"
    return
  fi

  return 1
}

resolve_absolute_path() {
  local path="$1"
  if [[ "$path" == /* ]]; then
    printf '%s\n' "$path"
    return
  fi
  printf '%s/%s\n' "$PWD" "$path"
}

expected_corpus_output_path() {
  local corpus_root="$1"
  local test_name="$2"
  printf '%s/%s\n' "$corpus_root" "$(sanitize_file_name "$test_name")"
}

expected_corpus_database_path() {
  local corpus_root="$1"
  local binary="$2"
  local test_name="$3"
  printf '%s/%s/%s/coverage\n' "$corpus_root" "$(basename "$binary")" "$test_name"
}

# Parse a FuzzTest log and emit a tab-separated summary line:
#   <elapsed>\t<runs>\t<edges>\t<crashes>\t<corpus>
# When several "Total runs / Edges covered / Corpus size" sections are present
# (multi-sibling aggregated log), we sum runs across siblings and take the
# MAX edges/corpus (siblings share the same coverage map and may report
# overlapping corpora). Elapsed reports the largest sibling wall clock.
parse_fuzz_log_summary() {
  local log_path="$1"
  if [[ ! -f "$log_path" ]]; then
    printf -- '-\t-\t-\t-\t-\n'
    return
  fi
  awk '
    function field_value(s) {
      gsub(/^[[:space:]]+|[[:space:]]+$/, "", s)
      return s
    }
    function as_number(v) {
      # FuzzTest may print runs as "3.46e+02" — convert to a plain integer.
      if (v == "" || v == "-") return 0
      return v + 0
    }
    function max(a, b) { return (a > b) ? a : b }
    BEGIN { current_runs = 0; total_runs = 0 }
    /sibling [0-9]+ \(/ {
      total_runs += current_runs
      current_runs = 0
    }
    /Corpus size:/ {
      n = split($0, pairs, "|")
      for (i = 1; i <= n; i++) {
        if (match(pairs[i], /Corpus size:[[:space:]]*[0-9]+/)) {
          v = substr(pairs[i], RSTART, RLENGTH)
          sub(/Corpus size:[[:space:]]*/, "", v)
          corpus = field_value(v)
        } else if (match(pairs[i], /Edges covered:[[:space:]]*[0-9]+/)) {
          v = substr(pairs[i], RSTART, RLENGTH)
          sub(/Edges covered:[[:space:]]*/, "", v)
          edges_now = field_value(v)
          if ((edges_now + 0) > (edges + 0)) edges = edges_now
        } else if (match(pairs[i], /Fuzzing time:[[:space:]]*[^ ]+/)) {
          v = substr(pairs[i], RSTART, RLENGTH)
          sub(/Fuzzing time:[[:space:]]*/, "", v)
          elapsed = field_value(v)
        } else if (match(pairs[i], /Total runs:[[:space:]]*[^ ]+/)) {
          v = substr(pairs[i], RSTART, RLENGTH)
          sub(/Total runs:[[:space:]]*/, "", v)
          current_runs = as_number(field_value(v))
        }
      }
    }
    /CRASH/ || /crash detected/ || /found a buggy input/ || /BUGGY[[:space:]]*INPUT/ || /AddressSanitizer/ || /UndefinedBehaviorSanitizer/ {
      crashes++
    }
    END {
      total_runs += current_runs
      if (total_runs == 0) runs = "-"; else runs = total_runs
      if (edges == "")   edges = "-"
      if (crashes == "") crashes = 0
      if (elapsed == "") elapsed = "-"
      if (corpus == "")  corpus = "-"
      printf "%s\t%s\t%s\t%s\t%s\n", elapsed, runs, edges, crashes, corpus
    }
  ' "$log_path"
}

# Walk the FuzzTest output dir and copy any saved input under findings/<test>/.
# Echoes the count of files copied.
extract_findings() {
  local out_dir="$1"
  local findings_dir="$2"
  local timestamp_tag="$3"
  if [[ ! -d "$out_dir" ]]; then
    printf '0\n'
    return
  fi
  mkdir -p "$findings_dir"
  local count=0
  while IFS= read -r -d '' input_file; do
    local base
    base="$(basename "$input_file")"
    local target="$findings_dir/${timestamp_tag}_${base}"
    cp -- "$input_file" "$target"
    {
      printf 'source: %s\n' "$input_file"
      local size
      size="$(stat -c '%s' "$input_file" 2>/dev/null || stat -f '%z' "$input_file" 2>/dev/null || printf '?')"
      printf 'size:   %s bytes\n' "$size"
      local sum
      sum="$(sha256sum "$input_file" 2>/dev/null | awk '{print $1}')"
      [[ -n "$sum" ]] && printf 'sha256: %s\n' "$sum"
    } >"${target}.txt"
    count=$((count + 1))
  done < <(find "$out_dir" -mindepth 1 -maxdepth 1 -type f -print0 2>/dev/null)
  printf '%s\n' "$count"
}

# Defaults
build_dir=""
binary_path=""
script_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd -- "$script_dir/.." && pwd)"
corpus_dir="$repo_root/.fuzz/out"
corpus_database_dir=""
total_jobs=""
time_budget=""
stack_limit_kb=256
continue_after_crash=0
print_subprocess_log=0
extract_findings_flag=0
summary_flag=0
color_enabled=1
config_name=""
preset_name=""
sequential_flag=0
force_flag=0
declare -a requested_tests=()
declare -a child_pids=()

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)            build_dir="$2"; shift 2 ;;
    --config)               config_name="$2"; shift 2 ;;
    --binary)               binary_path="$2"; shift 2 ;;
    --corpus)               corpus_dir="$2"; shift 2 ;;
    --corpus-database)      corpus_database_dir="$2"; shift 2 ;;
    --jobs)                 total_jobs="$2"; shift 2 ;;
    --time)                 time_budget="$2"; shift 2 ;;
    --stack-limit-kb)       stack_limit_kb="$2"; shift 2 ;;
    --test)                 requested_tests+=("$2"); shift 2 ;;
    --preset)               preset_name="$2"; shift 2 ;;
    --continue-after-crash) continue_after_crash=1; shift ;;
    --print-subprocess-log) print_subprocess_log=1; shift ;;
    --extract-findings)     extract_findings_flag=1; shift ;;
    --summary)              summary_flag=1; shift ;;
    --sequential)           sequential_flag=1; shift ;;
    --force)                force_flag=1; shift ;;
    --no-color)             color_enabled=0; shift ;;
    --help|-h)              usage; exit 0 ;;
    *)
      printf 'Unknown option: %s\n\n' "$1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

# Apply preset (only fills unset values; explicit flags win).
if [[ -n "$preset_name" ]]; then
  case "$preset_name" in
    smoke)
      [[ ${#requested_tests[@]} -eq 0 ]] && requested_tests=("all")
      [[ -z "$time_budget" ]] && time_budget="5m"
      ;;
    deep)
      [[ ${#requested_tests[@]} -eq 0 ]] && requested_tests=("all")
      [[ -z "$time_budget" ]] && time_budget="1h"
      [[ "$continue_after_crash" -eq 0 ]] && continue_after_crash=1
      summary_flag=1
      ;;
    campaign)
      [[ ${#requested_tests[@]} -eq 0 ]] && requested_tests=("all")
      [[ -z "$time_budget" ]] && time_budget="6h"
      [[ "$continue_after_crash" -eq 0 ]] && continue_after_crash=1
      extract_findings_flag=1
      summary_flag=1
      # Run tests sequentially in campaign mode: each test gets the FULL
      # job budget, avoiding the worker-explosion of N-tests × J-jobs
      # parallel workers fighting for cores on a typical machine.
      sequential_flag=1
      ;;
    *)
      printf 'Unknown preset: %s (expected smoke|deep|campaign)\n\n' "$preset_name" >&2
      usage >&2
      exit 1
      ;;
  esac
fi

# Resolve build dir from --config when --build-dir was not given.
if [[ -z "$build_dir" ]]; then
  if [[ -n "$config_name" ]]; then
    if ! build_dir="$(resolve_build_dir_for_config "$config_name")"; then
      exit 1
    fi
  else
    build_dir="build-fuzz-ci"
  fi
fi

if [[ -z "$total_jobs" ]]; then
  total_jobs="$(detect_default_jobs)"
fi

if [[ -z "$time_budget" ]]; then
  time_budget="10m"
fi

if [[ ! "$total_jobs" =~ ^[0-9]+$ || "$total_jobs" -lt 1 ]]; then
  printf -- '--jobs must be a positive integer.\n' >&2
  exit 1
fi

if [[ ! "$stack_limit_kb" =~ ^[0-9]+$ ]]; then
  printf -- '--stack-limit-kb must be a non-negative integer.\n' >&2
  exit 1
fi

if [[ ${#requested_tests[@]} -eq 0 ]]; then
  requested_tests=("pipeline" "build")
fi

declare -a resolved_tests=()
for requested in "${requested_tests[@]}"; do
  if [[ "$requested" == "all" ]]; then
    resolved_tests+=(
      "PegiumWorkspaceFuzzTest.StressLanguageSingleDocumentPipeline"
      "PegiumWorkspaceFuzzTest.StressLanguageArbitraryDocumentBuild"
      "PegiumWorkspaceFuzzTest.AdversarialLanguageSingleDocumentPipeline"
      "PegiumWorkspaceFuzzTest.AdversarialLanguageArbitraryDocumentBuild"
      "PegiumWorkspaceFuzzTest.AdversarialWorkspaceRelinkAndRecovery"
      "PegiumWorkspaceFuzzTest.StressLanguageIncrementalEditChain"
    )
    continue
  fi
  if [[ "$requested" == "all-examples" ]]; then
    resolved_tests+=(
      "PegiumExampleKeywordFuzzTest.ArithmeticsKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.DomainModelKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.StateMachineKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.RequirementsKeywordFuzz"
    )
    continue
  fi
  if [[ "$requested" == "all-everything" ]]; then
    resolved_tests+=(
      "PegiumWorkspaceFuzzTest.StressLanguageSingleDocumentPipeline"
      "PegiumWorkspaceFuzzTest.StressLanguageArbitraryDocumentBuild"
      "PegiumWorkspaceFuzzTest.AdversarialLanguageSingleDocumentPipeline"
      "PegiumWorkspaceFuzzTest.AdversarialLanguageArbitraryDocumentBuild"
      "PegiumWorkspaceFuzzTest.AdversarialWorkspaceRelinkAndRecovery"
      "PegiumWorkspaceFuzzTest.StressLanguageIncrementalEditChain"
      "PegiumExampleKeywordFuzzTest.ArithmeticsKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.DomainModelKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.StateMachineKeywordFuzz"
      "PegiumExampleKeywordFuzzTest.RequirementsKeywordFuzz"
    )
    continue
  fi
  resolved_tests+=("$(resolve_test_name "$requested")")
done

declare -A seen_tests=()
declare -a unique_tests=()
for test_name in "${resolved_tests[@]}"; do
  if [[ -n "${seen_tests[$test_name]:-}" ]]; then
    continue
  fi
  seen_tests["$test_name"]=1
  unique_tests+=("$test_name")
done
resolved_tests=("${unique_tests[@]}")

declare -A binary_paths_by_name=()

# Resolve a binary path for the given basename inside `build_dir`.
# Caches the result in binary_paths_by_name.
ensure_binary_for() {
  local binary_name="$1"
  if [[ -n "${binary_paths_by_name[$binary_name]:-}" ]]; then
    return 0
  fi
  local candidate
  candidate="$build_dir/tests/fuzz/$binary_name"
  if [[ ! -x "$candidate" ]]; then
    candidate="$(find "$build_dir" -type f -name "$binary_name" -print -quit 2>/dev/null || true)"
  fi
  if [[ -z "$candidate" || ! -x "$candidate" ]]; then
    printf 'Unable to find %s under %s.\n' "$binary_name" "$build_dir" >&2
    printf 'Build it first:\n' >&2
    printf '  cmake -DPEGIUM_BUILD_EXAMPLES=ON -S . -B %s\n' "$build_dir" >&2
    printf '  cmake --build %s --target %s -j32\n' "$build_dir" "$binary_name" >&2
    return 1
  fi
  binary_paths_by_name["$binary_name"]="$(resolve_absolute_path "$candidate")"
  return 0
}

# Backward-compat: if --binary was passed, bind it to the workspace binary
# basename. Otherwise resolve every required binary lazily below.
if [[ -n "$binary_path" ]]; then
  binary_path="$(resolve_absolute_path "$binary_path")"
  binary_paths_by_name["$(basename "$binary_path")"]="$binary_path"
fi

# Resolve all binaries required by the chosen test set.
declare -A required_binaries=()
for t in "${resolved_tests[@]}"; do
  bn="$(binary_for_test "$t")" || exit 1
  required_binaries["$bn"]=1
done
for bn in "${!required_binaries[@]}"; do
  if ! ensure_binary_for "$bn"; then
    exit 1
  fi
done

# For status messages, pick the first resolved binary as the canonical one.
if [[ -z "${binary_path:-}" ]]; then
  for bn in "${!binary_paths_by_name[@]}"; do
    binary_path="${binary_paths_by_name[$bn]}"
    break
  done
fi
corpus_dir="$(resolve_absolute_path "$corpus_dir")"
if [[ -n "$corpus_database_dir" ]]; then
  corpus_database_dir="$(resolve_absolute_path "$corpus_database_dir")"
fi

mkdir -p "$corpus_dir/logs"
findings_root="$corpus_dir/findings"
if [[ "$extract_findings_flag" -eq 1 ]]; then
  mkdir -p "$findings_root"
fi

# When jobs > 1, FuzzTest requires --corpus_database to be set for the
# `--config=fuzztest-experimental --jobs=N` Centipede multi-process mode
# to actually fork workers. Auto-provision one if the user didn't pass
# --corpus-database explicitly.
will_use_parallel_workers=0
if [[ "$total_jobs" -gt 1 ]]; then
  will_use_parallel_workers=1
fi
if [[ "$will_use_parallel_workers" -eq 1 && -z "$corpus_database_dir" ]]; then
  corpus_database_dir="$corpus_dir/db"
fi
if [[ -n "$corpus_database_dir" ]]; then
  mkdir -p "$corpus_database_dir"
fi

# Color helpers (used only in summary).
if [[ "$color_enabled" -eq 1 && -t 1 ]]; then
  c_red=$'\033[31m'; c_grn=$'\033[32m'; c_ylw=$'\033[33m'; c_dim=$'\033[2m'; c_rst=$'\033[0m'
else
  c_red=""; c_grn=""; c_ylw=""; c_dim=""; c_rst=""
fi

cleanup_children() {
  local pid
  # Disable bash's own job-control "Aborted/Terminated/Killed" status messages
  # for the children we're about to signal — the user got plenty of context
  # from our own logs and these noisy lines are just a distraction at cancel.
  set +m 2>/dev/null || true
  for pid in "${child_pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -TERM "$pid" 2>/dev/null || true
    fi
  done
  # Children get a brief grace period; SIGKILL anything still alive.
  sleep 0.5 2>/dev/null || true
  for pid in "${child_pids[@]}"; do
    if kill -0 "$pid" 2>/dev/null; then
      kill -KILL "$pid" 2>/dev/null || true
    fi
  done
}

trap cleanup_children INT TERM

# Safety check: refuse to start if another fuzz binary is already running.
# Multiple FuzzTest binaries on the same machine compete for cores AND for
# their own internal worker pool, slowing each other to a crawl. Use --force
# to bypass (e.g. across-config sweeps run from separate scripts).
binary_basename="$(basename "$binary_path")"
existing_pids="$(pgrep -af "$binary_basename --fuzz" 2>/dev/null | grep -v "^$$ " | awk '{print $1}' || true)"
if [[ -n "$existing_pids" ]]; then
  if [[ "$force_flag" -eq 1 ]]; then
    printf 'WARNING: %s already running (pid(s): %s); --force, continuing.\n' \
      "$binary_basename" "$(echo "$existing_pids" | tr '\n' ' ')" >&2
  else
    printf 'ABORT: %s is already running (pid(s): %s).\n' \
      "$binary_basename" "$(echo "$existing_pids" | tr '\n' ' ')" >&2
    printf 'Either wait for it to finish, kill it, or pass --force.\n' >&2
    exit 1
  fi
fi

run_started_at="$(date +%s)"
timestamp_tag="$(date -u +%Y%m%dT%H%M%SZ)"

printf 'Binary:        %s\n' "$binary_path"
printf 'Build config:  %s\n' "${config_name:-coverage (build-fuzz-ci)}"
printf 'Corpus root:   %s\n' "$corpus_dir"
if [[ "$extract_findings_flag" -eq 1 ]]; then
  printf 'Findings dir:  %s (auto-extract enabled)\n' "$findings_root"
fi
if [[ -n "$corpus_database_dir" ]]; then
  printf 'Corpus DB:     %s%s\n' "$corpus_database_dir" \
    "$([[ "$will_use_parallel_workers" -eq 1 ]] && printf ' (auto-provisioned for --jobs > 1)' || true)"
fi
printf 'Time budget:   %s (active fuzzing time, after seed replay/warm-up)\n' "$time_budget"
printf 'Stack limit:   %s KiB\n' "$stack_limit_kb"
printf 'Total jobs:    %s\n' "$total_jobs"
if [[ "$sequential_flag" -eq 1 ]]; then
  printf 'Dispatch:      sequential (each test runs alone with %s sibling(s))\n' "$total_jobs"
else
  per_test_jobs=$(( total_jobs / ${#resolved_tests[@]} ))
  if [[ "$per_test_jobs" -lt 1 ]]; then per_test_jobs=1; fi
  printf 'Dispatch:      parallel (%s sibling(s)/test, %s tests, %s total processes)\n' \
    "$per_test_jobs" "${#resolved_tests[@]}" "$((per_test_jobs * ${#resolved_tests[@]}))"
  if [[ "$per_test_jobs" -lt 4 && "${#resolved_tests[@]}" -ge 4 ]]; then
    printf '%sNOTE%s: with %s test(s) sharing %s jobs, each test gets only %s sibling(s).\n' \
      "$c_ylw" "$c_rst" "${#resolved_tests[@]}" "$total_jobs" "$per_test_jobs"
    printf '       For better per-test throughput, consider --sequential\n'
    printf '       (one test at a time with all %s siblings).\n' "$total_jobs"
  fi
fi
printf 'Tests:         %s\n' "${#resolved_tests[@]}"
for t in "${resolved_tests[@]}"; do
  printf '  - %s\n' "$t"
done

declare -A test_status=()
declare -A test_log_path=()
declare -A test_out_dir=()
declare -A test_findings_count=()

# Pre-register paths/binary for a test in the PARENT shell so post_run can
# read them — `run_one_test` may fork into a subshell (parallel dispatch)
# whose array writes never reach the parent.
register_test_paths() {
  local test_name="$1"
  test_log_path["$test_name"]="$corpus_dir/logs/$(sanitize_file_name "$test_name").log"
  test_out_dir["$test_name"]="$(expected_corpus_output_path "$corpus_dir" "$test_name")"
  mkdir -p "${test_out_dir[$test_name]}"
}

aggregate_sibling_logs() {
  local sibling_dir="$1"
  local log_path="$2"
  if [[ ! -d "$sibling_dir" ]]; then
    return 1
  fi

  local -a sibling_logs=()
  local sib_log
  for sib_log in "$sibling_dir"/*.log; do
    if [[ -f "$sib_log" ]]; then
      sibling_logs+=("$sib_log")
    fi
  done
  if [[ "${#sibling_logs[@]}" -eq 0 ]]; then
    return 1
  fi

  {
    printf '== Aggregated log: %s sibling(s) ==\n' "${#sibling_logs[@]}"
    local i
    for ((i = 0; i < ${#sibling_logs[@]}; ++i)); do
      printf '\n--- sibling %d (%s) ---\n' "$i" "${sibling_logs[$i]}"
      cat "${sibling_logs[$i]}" 2>/dev/null || true
    done
  } >"$log_path"
}

run_one_test() {
  local test_name="$1"
  local jobs_for_test="$2"
  local log_path="${test_log_path[$test_name]}"
  local out_dir="${test_out_dir[$test_name]}"
  local binary_name
  binary_name="$(binary_for_test "$test_name")"
  local binary_for_run="${binary_paths_by_name[$binary_name]}"

  # Build the base command. Each sibling will use a per-sibling
  # FUZZTEST_TESTSUITE_OUT_DIR so they don't clobber each other's saved
  # inputs. The corpus_database is per-BINARY (not shared across binaries)
  # because FuzzTest's DB layout assumes a single registered test set.
  local per_binary_db=""
  if [[ -n "$corpus_database_dir" ]]; then
    per_binary_db="$(corpus_database_for_binary "$corpus_database_dir" "$binary_name")"
    mkdir -p "$per_binary_db"
  fi
  local base_cmd=(
    "$binary_for_run"
    "--fuzz=$test_name"
    "--fuzz_for=$time_budget"
    "--stack_limit_kb=$stack_limit_kb"
  )
  if [[ -n "$per_binary_db" ]]; then
    base_cmd+=("--corpus_database=$per_binary_db")
  fi
  if [[ "$continue_after_crash" -eq 1 ]]; then
    base_cmd+=("--continue_after_crash")
  fi
  if [[ "$print_subprocess_log" -eq 1 ]]; then
    base_cmd+=("--print_subprocess_log")
  fi

  printf '\n[start] %s (siblings=%s)\n' "$test_name" "$jobs_for_test"
  printf '  log:  %s\n' "$log_path"
  printf '  out:  %s\n' "$out_dir"
  printf '  cmd:  %s\n' "${base_cmd[*]}"
  if [[ -n "$per_binary_db" ]]; then
    printf '  corpus_database (per-binary): %s\n' "$per_binary_db"
  fi

  if [[ "$jobs_for_test" -le 1 ]]; then
    env FUZZTEST_TESTSUITE_OUT_DIR="$out_dir" "${base_cmd[@]}" >"$log_path" 2>&1
    return $?
  fi

  # Multi-sibling fork. Each child gets its own out subdirectory and its
  # own log; the aggregate log is the head + all per-sibling tails.
  local sibling_dir="$out_dir/_siblings"
  mkdir -p "$sibling_dir"
  rm -f "$sibling_dir"/*.log
  local -a sibling_pids=()
  local -a sibling_logs=()
  local i
  for ((i = 0; i < jobs_for_test; ++i)); do
    local sib_out="$sibling_dir/$i"
    local sib_log="$sibling_dir/$i.log"
    mkdir -p "$sib_out"
    sibling_logs+=("$sib_log")
    env FUZZTEST_TESTSUITE_OUT_DIR="$sib_out" \
        "${base_cmd[@]}" >"$sib_log" 2>&1 &
    sibling_pids+=("$!")
  done

  local rc_overall=0
  local sib_rc
  for ((i = 0; i < ${#sibling_pids[@]}; ++i)); do
    set +e
    wait "${sibling_pids[$i]}"
    sib_rc=$?
    set -e
    if [[ "$sib_rc" -ne 0 ]]; then
      rc_overall=1
    fi
  done

  # Aggregate sibling logs into the canonical log file. Keep them
  # individually under _siblings/ so post-mortems can inspect each one.
  aggregate_sibling_logs "$sibling_dir" "$log_path"

  return "$rc_overall"
}

post_run_one_test() {
  local test_name="$1"
  local exit_status="$2"
  local log_path="${test_log_path[$test_name]}"
  local out_dir="${test_out_dir[$test_name]}"
  test_status["$test_name"]="$exit_status"

  if [[ -d "$out_dir/_siblings" ]]; then
    aggregate_sibling_logs "$out_dir/_siblings" "$log_path" || true
  fi

  if [[ "$exit_status" -ne 0 ]]; then
    printf '\n%s[fail]%s %s (exit=%s)\n' "$c_red" "$c_rst" "$test_name" "$exit_status" >&2
    tail -n 40 "$log_path" >&2 || true
  else
    printf '\n%s[ok]%s %s\n' "$c_grn" "$c_rst" "$test_name"
    tail -n 5 "$log_path" || true
  fi

  # Coverage check: any corpus file under the per-test out dir, OR under any
  # _siblings/<i>/ subdir (multi-sibling mode). FuzzTest persists corpus into
  # the test out dir; siblings each have their own subdir.
  if ! find "$out_dir" -mindepth 1 -type f -print -quit 2>/dev/null | grep -q .; then
    printf '%s[warn]%s no corpus files materialized under %s\n' \
      "$c_ylw" "$c_rst" "$out_dir" >&2
  fi
  # NOTE: we deliberately don't probe the corpus_database internal layout —
  # FuzzTest does not always create the legacy `coverage/` subdirectory, and
  # the noisy warning was misleading. The presence of fuzz output above is
  # the correct signal that the run produced data.

  if [[ "$extract_findings_flag" -eq 1 ]]; then
    local findings_dir="$findings_root/$(sanitize_file_name "$test_name")"
    local copied
    copied="$(extract_findings "$out_dir" "$findings_dir" "$timestamp_tag")"
    test_findings_count["$test_name"]="$copied"
    if [[ "$copied" -gt 0 ]]; then
      printf '%s[findings]%s %s saved to %s\n' "$c_ylw" "$c_rst" "$copied" "$findings_dir"
    fi
  fi
}

print_summary() {
  printf '\n%s================ Fuzz campaign summary ================%s\n' "$c_dim" "$c_rst"
  local total_secs=$(( $(date +%s) - run_started_at ))
  printf 'Total wall clock: %ds\n' "$total_secs"
  printf 'Findings extract: %s\n' "$([[ "$extract_findings_flag" -eq 1 ]] && echo enabled || echo disabled)"
  printf '\n%-72s %-6s %-12s %-10s %-9s %-7s %-7s %-9s\n' \
    "Test" "Status" "Elapsed" "Runs" "Edges" "Corpus" "Crashes" "Findings"
  printf -- '------------------------------------------------------------------------------------------------------------------------------------\n'
  for test_name in "${resolved_tests[@]}"; do
    local status="${test_status[$test_name]:-?}"
    local status_str
    if [[ "$status" == "0" ]]; then
      status_str="${c_grn}OK${c_rst}"
    else
      status_str="${c_red}FAIL${c_rst}"
    fi
    local log_path="${test_log_path[$test_name]:-}"
    local summary_line
    summary_line="$(parse_fuzz_log_summary "$log_path")"
    local elapsed runs edges crashes corpus
    IFS=$'\t' read -r elapsed runs edges crashes corpus <<<"$summary_line"
    local findings_count="${test_findings_count[$test_name]:-0}"
    printf '%-72s %-6b %-12s %-10s %-9s %-7s %-7s %-9s\n' \
      "$test_name" "$status_str" "$elapsed" "$runs" "$edges" "$corpus" "$crashes" "$findings_count"
  done
  printf '%s========================================================%s\n' "$c_dim" "$c_rst"
}

# --- Sequential dispatch ---------------------------------------------------
# Forced when --sequential is set OR when total_jobs < num_tests (otherwise
# each parallel test would get 0 workers).
if [[ "$sequential_flag" -eq 1 || "$total_jobs" -lt "${#resolved_tests[@]}" ]]; then
  if [[ "$total_jobs" -lt "${#resolved_tests[@]}" && "$sequential_flag" -eq 0 ]]; then
    printf '\nJobs (%s) < tests (%s). Falling back to sequential dispatch.\n' \
      "$total_jobs" "${#resolved_tests[@]}"
  fi
  overall_status=0
  for test_name in "${resolved_tests[@]}"; do
    register_test_paths "$test_name"
    set +e
    run_one_test "$test_name" "$total_jobs"
    rc=$?
    set -e
    post_run_one_test "$test_name" "$rc"
    if [[ "$rc" -ne 0 ]]; then
      overall_status=1
    fi
  done
  if [[ "$summary_flag" -eq 1 ]]; then
    print_summary
  fi
  exit "$overall_status"
fi

# --- Parallel dispatch (one process per test, jobs split among them) ------
base_jobs=$(( total_jobs / ${#resolved_tests[@]} ))
extra_jobs=$(( total_jobs % ${#resolved_tests[@]} ))

declare -a child_test_names=()

for index in "${!resolved_tests[@]}"; do
  test_name="${resolved_tests[$index]}"
  jobs_for_test="$base_jobs"
  if [[ "$index" -lt "$extra_jobs" ]]; then
    jobs_for_test=$(( jobs_for_test + 1 ))
  fi
  if [[ "$jobs_for_test" -lt 1 ]]; then
    jobs_for_test=1
  fi
  register_test_paths "$test_name"
  run_one_test "$test_name" "$jobs_for_test" &
  child_pids+=("$!")
  child_test_names+=("$test_name")
done

overall_status=0
for index in "${!child_pids[@]}"; do
  pid="${child_pids[$index]}"
  test_name="${child_test_names[$index]}"
  set +e
  wait "$pid"
  rc=$?
  set -e
  post_run_one_test "$test_name" "$rc"
  if [[ "$rc" -ne 0 ]]; then
    overall_status=1
  fi
done

if [[ "$summary_flag" -eq 1 ]]; then
  print_summary
fi

exit "$overall_status"
