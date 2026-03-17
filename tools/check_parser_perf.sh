#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN_PATH="${1:-"$ROOT_DIR/build/PegiumLegacyLanguagesTest"}"
THRESHOLD_BPS="${PEGIUM_CALLGRIND_THRESHOLD_BPS:-100}"
WORK_DIR="${TMPDIR:-/tmp}/pegium-parser-perf"
BIN_DIR="$(cd "$(dirname "$BIN_PATH")" && pwd)"
BIN_NAME="$(basename "$BIN_PATH")"
BUILD_JOBS="${PEGIUM_BUILD_JOBS:-32}"

mkdir -p "$WORK_DIR"

if [[ "${PEGIUM_PERF_REBUILD:-1}" != "0" ]]; then
  if [[ -f "$BIN_DIR/CMakeCache.txt" && -f "$BIN_DIR/build.ninja" ]] ||
     [[ -f "$BIN_DIR/CMakeCache.txt" && -f "$BIN_DIR/Makefile" ]]; then
    cmake --build "$BIN_DIR" --target "$BIN_NAME" -j"$BUILD_JOBS"
  fi
fi

if [[ ! -x "$BIN_PATH" ]]; then
  echo "Missing benchmark executable: $BIN_PATH" >&2
  exit 2
fi

if ! command -v valgrind >/dev/null 2>&1; then
  echo "valgrind is required to run parser perf checks." >&2
  exit 2
fi

cases=(
  "ArithmeticsBenchmark.ParseSpeedMicroBenchmark|160459973"
  "XsmpBenchmark.ParseSpeedMicroBenchmark|317430536"
  "XsmpBenchmark.ParseSpeedMicroBenchmarkCatalogueTypo|316518671"
  "JsonBenchmark.ParseSpeedMicroBenchmark|222900275"
  "InfixRuleBenchmark.HasOperatorProbeMicroBenchmark|79894532"
  "RepetitionBenchmark.ParseSpeedMicroBenchmark|31462307"
  "ExpectFrontierBenchmark.ExpectSpeedMicroBenchmark|14065738"
)

status=0

printf "%-52s %14s %14s %10s\n" "Benchmark" "Baseline Ir" "Actual Ir" "Delta"

for entry in "${cases[@]}"; do
  filter="${entry%%|*}"
  baseline="${entry##*|}"
  case_slug="${filter//./_}"
  callgrind_out="$WORK_DIR/${case_slug}.callgrind"
  log_out="$WORK_DIR/${case_slug}.log"

  rm -f "$callgrind_out" "$log_out"
  env \
    PEGIUM_BENCH_ITERATIONS=1 \
    PEGIUM_BENCH_WARMUP=0 \
    PEGIUM_BENCH_SAMPLES=1 \
    valgrind \
      --tool=callgrind \
      --callgrind-out-file="$callgrind_out" \
      "$BIN_PATH" \
      --gtest_filter="$filter" \
      --gtest_brief=1 \
      >"$log_out" 2>&1

  summary_line="$(grep -m1 '^summary:' "$callgrind_out" || true)"
  if [[ -z "$summary_line" ]]; then
    echo "Failed to extract callgrind summary for $filter" >&2
    cat "$log_out" >&2
    exit 2
  fi

  actual="${summary_line#summary: }"
  delta=$(( actual - baseline ))
  limit=$(( baseline * (10000 + THRESHOLD_BPS) / 10000 ))

  if (( delta >= 0 )); then
    delta_text="+$delta"
  else
    delta_text="$delta"
  fi

  printf "%-52s %14d %14d %10s\n" "$filter" "$baseline" "$actual" "$delta_text"

  if (( actual > limit )); then
    status=1
  fi
done

if (( status != 0 )); then
  echo "Parser perf check failed: at least one benchmark regressed by more than $((THRESHOLD_BPS)) bps." >&2
  exit 1
fi

echo "Parser perf check passed."
