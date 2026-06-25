#!/usr/bin/env python3

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path


# Single-file example benchmarks, paired by exact name.
SINGLE_BENCHMARKS = (
    "arithmetics",
    "domainmodel",
    "requirements",
    "statemachine",
)

# Per-language workspace benchmarks (one small + one large workspace each),
# reported as full build only. Paired after stripping the " files=N" suffix.
WORKSPACE_BENCHMARKS = tuple(
    f"{language}-workspace-{size}"
    for language in ("arithmetics", "domainmodel", "requirements", "statemachine")
    for size in ("small", "large")
)

STEPS = (
    "parse+index",
    "scope+link",
    "validate",
    "full-build",
)

# The bench harness lives in this repository and is installed into the Langium
# checkout, so it can never be lost again.
HARNESS_SOURCE = Path(__file__).resolve().parent / "langium-bench" / "bench-examples.mjs"
LANGIUM_GIT_URL = "https://github.com/eclipse-langium/langium.git"

BENCH_HEADER_RE = re.compile(r"^\[bench\]\s+(.+?)\s+size=(\d+)B(?:\s+iterations=(\d+))?\s*$")
# Step line: "  <name>   <ms>ms  <throughput>MiB/s" — tolerant of the spacing
# differences between PegiumBench (iostream padding) and the Langium harness.
# Step names may contain '+' (e.g. "parse+index").
STEP_RE = re.compile(r"^\s+([a-z+-]+)\s+([0-9]+(?:\.[0-9]+)?)\s*ms\b.*$")


@dataclass(frozen=True)
class BenchResult:
    name: str
    size_bytes: int
    iterations: int | None
    steps: dict[str, float]


def normalize_name(name: str) -> str:
    # "workspace-small files=67" -> "workspace-small"
    return name.split(" files=", 1)[0]


def parse_args() -> argparse.Namespace:
    pegium_repo = Path(__file__).resolve().parent.parent

    parser = argparse.ArgumentParser(
        description="Compare PegiumBench and Langium example benches in one command."
    )
    parser.add_argument(
        "--pegium-bench",
        default=str(pegium_repo / "build" / "tests" / "bench" / "PegiumBench"),
        help="Path to the PegiumBench executable.",
    )
    parser.add_argument(
        "--langium-repo",
        default=str(pegium_repo.parent / "langium"),
        help="Path to the Langium repository.",
    )
    parser.add_argument(
        "--setup",
        action="store_true",
        help="Clone Langium (if missing), run npm ci && npm run build, and install "
        "the bench harness before running.",
    )
    parser.add_argument(
        "--clone",
        action="store_true",
        help="Force a fresh clone of Langium into --langium-repo (implies --setup; "
        "fails if the directory already exists).",
    )
    parser.add_argument(
        "--langium-ref",
        default=None,
        help="Git branch/tag to clone (default: the repository's default branch, "
        "i.e. the latest Langium).",
    )
    parser.add_argument("--bytes", type=int, default=64 * 1024,
                        help="Single-file benchmark input size in bytes.")
    parser.add_argument("--ws-small", type=int, default=256 * 1024,
                        help="Small-workspace target size in bytes.")
    parser.add_argument("--ws-large", type=int, default=12 * 1024 * 1024,
                        help="Large-workspace target size in bytes.")
    parser.add_argument("--iterations", type=int, default=3,
                        help="Number of benchmark iterations.")
    parser.add_argument("--warmup", type=int, default=0,
                        help="Number of Langium warmup iterations.")
    parser.add_argument("--memory", action=argparse.BooleanOptionalAction,
                        default=True,
                        help="Also measure peak resident memory (RSS) per "
                             "workspace config, each in an isolated run "
                             "(adds 8 short runs per engine).")
    return parser.parse_args()


def fail(message: str) -> None:
    print(f"Error: {message}", file=sys.stderr)
    raise SystemExit(1)


def run_command(command: list[str], *, cwd: Path,
                env: dict[str, str] | None = None, capture: bool = True) -> str:
    process = subprocess.run(
        command, cwd=cwd, env=env, capture_output=capture, text=True, check=False,
    )
    output = ""
    if capture:
        output = process.stdout
        if process.stderr:
            if output and not output.endswith("\n"):
                output += "\n"
            output += process.stderr
    if process.returncode != 0:
        message = (output.strip() if capture else "") or \
            f"Command failed with exit code {process.returncode}."
        fail(f"Command {' '.join(command)} failed in '{cwd}'.\n{message}")
    return output


def run_bench(command: list[str], *, cwd: Path, env: dict[str, str],
              label: str) -> str:
    # Capture stdout (the machine-readable results, parsed below) but let the
    # child's stderr stream straight to our terminal, so its per-benchmark
    # progress traces show up live. Without this, a slow benchmark makes the
    # whole run look frozen until it finishes.
    print(f"==> {label} ...", file=sys.stderr, flush=True)
    process = subprocess.run(
        command, cwd=cwd, env=env, stdout=subprocess.PIPE, stderr=None,
        text=True, check=False,
    )
    if process.returncode != 0:
        fail(f"Command {' '.join(command)} failed in '{cwd}' "
             f"(exit code {process.returncode}); see the trace above.")
    return process.stdout


def install_harness(langium_repo: Path) -> None:
    if not HARNESS_SOURCE.is_file():
        fail(f"Bench harness not found at '{HARNESS_SOURCE}'.")
    scripts_dir = langium_repo / "scripts"
    scripts_dir.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(HARNESS_SOURCE, scripts_dir / "bench-examples.mjs")


def setup_langium(langium_repo: Path, *, clone: bool, ref: str | None) -> None:
    if shutil.which("git") is None or shutil.which("npm") is None:
        fail("git and npm are required for --setup/--clone.")

    if clone or not langium_repo.exists():
        if langium_repo.exists():
            fail(f"'{langium_repo}' already exists; remove it or pick another "
                 "--langium-repo before cloning.")
        command = ["git", "clone", "--depth", "1"]
        if ref:
            command += ["--branch", ref]
        command += [LANGIUM_GIT_URL, str(langium_repo)]
        print(f"Cloning Langium into '{langium_repo}'...", file=sys.stderr)
        run_command(command, cwd=langium_repo.parent, capture=False)
        print("Installing dependencies (npm ci)...", file=sys.stderr)
        run_command(["npm", "ci"], cwd=langium_repo, capture=False)
        print("Building Langium and examples (npm run build)...", file=sys.stderr)
        run_command(["npm", "run", "build"], cwd=langium_repo, capture=False)

    install_harness(langium_repo)


def ensure_pegium_bench(path: Path) -> None:
    if not path.is_file() or not os.access(path, os.X_OK):
        fail(f"PegiumBench not found or not executable at '{path}'. Build it with: "
             "cmake --build build -j --target PegiumBench")


def ensure_langium_repo(path: Path) -> None:
    if not (path / "package.json").is_file():
        fail(f"Langium repository not found at '{path}'. Run with --setup to clone "
             "and build it automatically.")
    build_outputs = (
        path / "packages" / "langium" / "lib" / "index.js",
        path / "examples" / "arithmetics" / "out" / "language-server" / "arithmetics-module.js",
        path / "examples" / "domainmodel" / "out" / "language-server" / "domain-model-module.js",
        path / "examples" / "requirements" / "out" / "language-server" / "requirements-and-tests-lang-module.js",
        path / "examples" / "statemachine" / "out" / "language-server" / "statemachine-module.js",
    )
    missing = [output for output in build_outputs if not output.is_file()]
    if missing:
        fail(f"Langium build outputs are missing (first: '{missing[0]}'). "
             "Run with --setup, or 'npm ci && npm run build' in the Langium repo.")


def parse_bench_output(output: str, label: str) -> dict[str, BenchResult]:
    results: dict[str, BenchResult] = {}
    name: str | None = None
    size: int | None = None
    iterations: int | None = None
    steps: dict[str, float] = {}

    def finalize() -> None:
        nonlocal name, size, iterations, steps
        if name is not None and steps:
            results[normalize_name(name)] = BenchResult(
                name=name, size_bytes=size or 0, iterations=iterations,
                steps=dict(steps))
        name, size, iterations, steps = None, None, None, {}

    for raw_line in output.splitlines():
        header = BENCH_HEADER_RE.match(raw_line.rstrip())
        if header:
            finalize()
            name = header.group(1)
            size = int(header.group(2))
            iterations = int(header.group(3)) if header.group(3) else None
            continue
        step = STEP_RE.match(raw_line.rstrip())
        if step and name is not None and step.group(1) in STEPS:
            steps[step.group(1)] = float(step.group(2))
    finalize()

    required = list(SINGLE_BENCHMARKS) + list(WORKSPACE_BENCHMARKS)
    missing = [bench for bench in required if bench not in results]
    if missing:
        fail(f"Missing benchmarks {missing} in {label} output.")
    return results


def ratio(langium_ms: float, pegium_ms: float) -> str:
    return f"{langium_ms / pegium_ms:.1f}x" if pegium_ms > 0 else "n/a"


def mib_per_second(size_bytes: int, ms: float) -> float:
    if ms <= 0:
        return 0.0
    return (size_bytes / (1024 * 1024)) / (ms / 1000.0)


def measure_peak_rss(command: list[str], *, cwd: Path,
                     env: dict[str, str]) -> int | None:
    """Run `command` to completion and return its peak resident set size in
    KiB (Linux `ru_maxrss`), or None on failure. Output is discarded — only the
    process's peak memory matters here."""
    process = subprocess.Popen(command, cwd=cwd, env=env,
                               stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)
    _, status, usage = os.wait4(process.pid, 0)
    if status != 0:
        return None
    return usage.ru_maxrss


def collect_workspace_memory(
        pegium_bench: Path, langium_repo: Path, *, args: argparse.Namespace
) -> dict[str, tuple[int | None, int | None]]:
    """Peak RSS (KiB) per workspace config for each engine, measured one config
    at a time so a config's footprint is not polluted by the other workspaces.
    Both benches gate input generation on their *_BENCH_FILTER env, so a filtered
    run only builds the one workspace."""
    memory: dict[str, tuple[int | None, int | None]] = {}
    for name in WORKSPACE_LANGUAGES:
        for suffix in ("small", "large"):
            config = f"{name}-workspace-{suffix}"
            print(f"==> measuring memory: {config} ...", file=sys.stderr,
                  flush=True)
            pegium_env = os.environ.copy()
            pegium_env.update(
                PEGIUM_BENCH_FILTER=config, PEGIUM_BENCH_ITERATIONS="1",
                PEGIUM_BENCH_WS_SMALL=str(args.ws_small),
                PEGIUM_BENCH_WS_LARGE=str(args.ws_large),
            )
            langium_env = os.environ.copy()
            langium_env.update(
                LANGIUM_BENCH_FILTER=config, LANGIUM_BENCH_ITERATIONS="1",
                LANGIUM_BENCH_WARMUP="0",
                LANGIUM_BENCH_WS_SMALL=str(args.ws_small),
                LANGIUM_BENCH_WS_LARGE=str(args.ws_large),
            )
            pegium_kib = measure_peak_rss(
                [str(pegium_bench)], cwd=pegium_bench.parent, env=pegium_env)
            langium_kib = measure_peak_rss(
                ["node", "scripts/bench-examples.mjs"], cwd=langium_repo,
                env=langium_env)
            memory[config] = (pegium_kib, langium_kib)
    return memory


WORKSPACE_LANGUAGES = ("arithmetics", "domainmodel", "requirements", "statemachine")


def print_summary(pegium: dict[str, BenchResult], langium: dict[str, BenchResult],
                  *, args: argparse.Namespace, pegium_bench: Path,
                  langium_repo: Path,
                  memory: dict[str, tuple[int | None, int | None]] | None = None
                  ) -> None:
    print("# Pegium vs Langium bench comparison")
    print()
    print(f"_Parameters_: single bytes={args.bytes}, ws-small={args.ws_small}, "
          f"ws-large={args.ws_large}, iterations={args.iterations}")
    print(f"_Sources_: pegium=`{pegium_bench}`, langium=`{langium_repo}`")
    if memory is not None:
        print("_Memory_: peak resident set size (RSS) per workspace config, "
              "each measured in an isolated run. RSS includes the runtime "
              "baseline — Node/V8 carries a fixed ~tens-of-MiB heap the native "
              "pegium process does not, so weigh growth across sizes as much as "
              "the absolute numbers.")
    print()

    def fmt_size(num_bytes: int) -> str:
        if num_bytes >= 1024 * 1024:
            return f"{num_bytes / (1024 * 1024):.1f} MiB"
        return f"{num_bytes / 1024:.0f} KiB"

    def fmt_mib(kib: int | None) -> str:
        return f"{kib / 1024:.0f} MiB" if kib else "n/a"

    def mem_ratio(langium_kib: int | None, pegium_kib: int | None) -> str:
        if not langium_kib or not pegium_kib:
            return "n/a"
        return f"{langium_kib / pegium_kib:.1f}x"

    def files_in(name: str) -> int | None:
        _, sep, tail = name.partition(" files=")
        token = tail.split(" ", 1)[0] if sep else ""
        return int(token) if token.isdigit() else None

    def throughput_table(title: str, rows: list[tuple[str, str]], *,
                         with_memory: bool = False,
                         workspace: bool = False) -> None:
        # Each row: (label, result key). Columns: build time + throughput for
        # both engines, plus the langium/pegium speedup; workspace tables add
        # peak-RSS columns when memory was measured, and a header line with the
        # number of files handed to the document builder at once.
        if workspace:
            counts = sorted({c for _, key in rows
                             if (c := files_in(pegium[key].name)) is not None})
            if counts:
                shown = (str(counts[0]) if len(counts) == 1
                         else f"{counts[0]}–{counts[-1]}")
                title = f"{title} ({shown} files built simultaneously at startup)"
        print(f"## {title}")
        print()
        header = ("| language | size | pegium time | pegium throughput | "
                  "langium time | langium throughput | speedup |")
        divider = "| --- | ---: | ---: | ---: | ---: | ---: | ---: |"
        if with_memory:
            header += " pegium RSS | langium RSS | RSS ratio |"
            divider += " ---: | ---: | ---: |"
        print(header)
        print(divider)
        for label, key in rows:
            p, l = pegium[key], langium[key]
            p_ms, l_ms = p.steps["full-build"], l.steps["full-build"]
            row = (f"| {label} | {fmt_size(p.size_bytes)} | "
                   f"{p_ms:.2f} ms | {mib_per_second(p.size_bytes, p_ms):.1f} MiB/s | "
                   f"{l_ms:.2f} ms | {mib_per_second(l.size_bytes, l_ms):.1f} MiB/s | "
                   f"{ratio(l_ms, p_ms)} |")
            if with_memory:
                p_kib, l_kib = (memory or {}).get(key, (None, None))
                row += (f" {fmt_mib(p_kib)} | {fmt_mib(l_kib)} | "
                        f"{mem_ratio(l_kib, p_kib)} |")
            print(row)
        print()

    throughput_table("Single-file full build",
                     [(name, name) for name in SINGLE_BENCHMARKS])
    throughput_table("Workspace ~250 KB",
                     [(name, f"{name}-workspace-small")
                      for name in WORKSPACE_LANGUAGES],
                     with_memory=memory is not None, workspace=True)
    throughput_table("Workspace ~12 MB",
                     [(name, f"{name}-workspace-large")
                      for name in WORKSPACE_LANGUAGES],
                     with_memory=memory is not None, workspace=True)

    # Per-phase breakdown for the single-file benches (the three real build phases).
    for key in SINGLE_BENCHMARKS:
        print(f"## {key} (per phase)")
        print()
        print("| phase | pegium time | langium time | speedup |")
        print("| --- | ---: | ---: | ---: |")
        for step in STEPS:
            p_ms = pegium[key].steps[step]
            l_ms = langium[key].steps[step]
            print(f"| {step} | {p_ms:.2f} ms | {l_ms:.2f} ms | {ratio(l_ms, p_ms)} |")
        print()


def main() -> None:
    args = parse_args()
    pegium_bench = Path(args.pegium_bench).resolve()
    langium_repo = Path(args.langium_repo).resolve()

    ensure_pegium_bench(pegium_bench)
    if args.setup or args.clone:
        setup_langium(langium_repo, clone=args.clone, ref=args.langium_ref)
    elif langium_repo.exists():
        # Always (re)install the harness so the comparison cannot drift or get lost.
        install_harness(langium_repo)
    ensure_langium_repo(langium_repo)

    pegium_env = os.environ.copy()
    pegium_env.update(
        PEGIUM_BENCH_BYTES=str(args.bytes),
        PEGIUM_BENCH_ITERATIONS=str(args.iterations),
        PEGIUM_BENCH_WS_SMALL=str(args.ws_small),
        PEGIUM_BENCH_WS_LARGE=str(args.ws_large),
    )
    langium_env = os.environ.copy()
    langium_env.update(
        LANGIUM_BENCH_BYTES=str(args.bytes),
        LANGIUM_BENCH_ITERATIONS=str(args.iterations),
        LANGIUM_BENCH_WARMUP=str(args.warmup),
        LANGIUM_BENCH_WS_SMALL=str(args.ws_small),
        LANGIUM_BENCH_WS_LARGE=str(args.ws_large),
    )

    pegium_output = run_bench([str(pegium_bench)], cwd=pegium_bench.parent,
                              env=pegium_env, label="Running PegiumBench")
    langium_output = run_bench(["node", "scripts/bench-examples.mjs"],
                               cwd=langium_repo, env=langium_env,
                               label="Running Langium bench")

    memory = None
    if args.memory:
        memory = collect_workspace_memory(pegium_bench, langium_repo, args=args)

    print_summary(
        parse_bench_output(pegium_output, "PegiumBench"),
        parse_bench_output(langium_output, "Langium bench"),
        args=args, pegium_bench=pegium_bench, langium_repo=langium_repo,
        memory=memory,
    )


if __name__ == "__main__":
    main()
