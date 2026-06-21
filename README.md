<div align="center">
  <h1>Pegium</h1>
  <h3>C++20 language engineering toolkit</h3>
</div>

<div align="center">

  [![CI](https://github.com/ydaveluy/pegium/actions/workflows/ci.yml/badge.svg)](https://github.com/ydaveluy/pegium/actions/workflows/ci.yml)
  [![Documentation](https://github.com/ydaveluy/pegium/actions/workflows/docs.yml/badge.svg)](https://github.com/ydaveluy/pegium/actions/workflows/docs.yml)
  [![SonarCloud analysis](https://github.com/ydaveluy/pegium/actions/workflows/sonarcloud.yml/badge.svg)](https://github.com/ydaveluy/pegium/actions/workflows/sonarcloud.yml)
  [![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=ydaveluy_pegium&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=ydaveluy_pegium)

</div>

---

Pegium is a language engineering toolkit for C++20 with built-in support for
parsing, AST/CST construction, references, validation, formatting, and
language-server features.

Pegium is strongly inspired by
[Langium](https://github.com/eclipse-langium/langium), and many of the core
concepts are intentionally similar. The main difference is that Pegium centers
on a PEG-based parser DSL in C++, instead of Langium's TypeScript grammar and
parser stack.

* **Semantics First:** Pegium lets you shape the semantic model of your
  language directly through C++ AST types plus grammar assignments, while still
  keeping CST data available for source-aware tooling.
* **Explicit Services, Customizable by Design:** Pegium exposes parser,
  scoping, validation, workspace, formatting, and LSP behavior through visible
  service objects instead of hiding the wiring behind heavy code generation.
* **Parser to Editor in One Toolkit:** The same document model supports parsing,
  linking, diagnostics, formatting, completion, rename, references, and other
  editor features.

## Installation

**Prerequisites:** a C++20 compiler, CMake 3.14 or later. Node.js is only
needed if you want to build the VS Code extension (`-DVSCODE=ON`, the default).

Scaffold a new language with a single command — no cloning required:

```bash
curl -fsSLO https://ydaveluy.github.io/pegium/pegium-new.cmake && \
  cmake -DNAME=MyLang -DEXT=.ml -P pegium-new.cmake
cd mylang && cmake -B build && cmake --build build -j
./build/mylang-cli example/hello.ml
```

The script creates a `mylang/` directory with a working "Hello world" grammar,
CLI, LSP server, and VS Code extension, pulling Pegium in via `FetchContent`.

### Scaffolding flags

| Flag | Default | Description |
|------|---------|-------------|
| `NAME` | *(required)* | PascalCase C++ identifier for your language (e.g. `MyLang`) |
| `EXT` | `.<lowercased-name>` | File extension, must start with `.` (e.g. `-DEXT=.ml`) |
| `DIR` | `<lowercased-name>` | Output directory (e.g. `-DDIR=my-project`) |
| `LSP` | `ON` | Build the LSP server; pass `-DLSP=OFF` to skip |
| `VSCODE` | `ON` | Scaffold the VS Code extension; pass `-DVSCODE=OFF` to skip |
| `CLI` | `ON` | Build the CLI tool; pass `-DCLI=OFF` to skip |
| `PEGIUM_TAG` | `main` | Pegium tag/commit to pin (e.g. `-DPEGIUM_TAG=v1.2.0`) |

### Try the shipped examples

Open the repository root in VS Code, go to `Run and Debug`, pick one of
`Run Arithmetics Extension`, `Run DomainModel Extension`,
`Run Requirements Extension`, or `Run Statemachine Extension`, then press
`F5`.

On the first launch, VS Code runs the matching `Prepare ... Extension` task for
you: it configures CMake, builds the example language server, installs the
extension dependencies if needed, and compiles the VS Code extension.

VS Code then opens a new Extension Development Host window on the corresponding
example workspace, so you can immediately try the language features on the
shipped sample files.

If you are new to the project, the best documentation entry points are:

- [Introduction](https://ydaveluy.github.io/pegium/introduction/)
- [Learn Pegium](https://ydaveluy.github.io/pegium/learn/)
- [Recipes](https://ydaveluy.github.io/pegium/recipes/)
- [Reference](https://ydaveluy.github.io/pegium/reference/)
- [Examples Overview](https://ydaveluy.github.io/pegium/examples/)

## Documentation

You can find the Pegium documentation on
[the documentation website](https://ydaveluy.github.io/pegium/).

The documentation is organized into several sections:

- [Introduction](https://ydaveluy.github.io/pegium/introduction/): what Pegium
  is, why it exists, and how it relates to Langium
- [Learn](https://ydaveluy.github.io/pegium/learn/): the recommended workflow
  for building a language with Pegium
- [Recipes](https://ydaveluy.github.io/pegium/recipes/): targeted guides for
  customization tasks such as scoping, validation, caching, and multiple
  languages
- [Reference](https://ydaveluy.github.io/pegium/reference/): canonical
  documentation for grammar, services, semantic model, and document lifecycle
- [Examples](https://ydaveluy.github.io/pegium/examples/): the shipped example
  languages and what each one demonstrates

The documentation sources live in [docs/](docs/index.md) in this repository.

## Examples

Pegium ships several end-to-end examples in this repository:

- **[arithmetics](examples/arithmetics/README.md)**: a compact expression
  language with evaluator, formatter, CLI, and LSP server
- **[DomainModel](examples/domainmodel/README.md)**: a modeling DSL with
  qualified names, formatter rules, and rename support
- **[requirements](examples/requirements/README.md)**: a multi-language example
  showing shared workspace behavior and cross-language references
- **[statemachine](examples/statemachine/README.md)**: a modeling language that
  emphasizes validation and editor integration

## Benchmarks

Pegium and [Langium](https://github.com/eclipse-langium/langium) ship the same
four example languages, so they can be compared directly. Each language is built
through the full document pipeline (parse → index → scope → link → validate) from
**byte-identical** generated inputs, averaged over 3 iterations. The workspace
benchmarks hand many self-contained files of one language to the framework's
document builder **at once**, as a single small (~250 KB) and large (~12 MB)
startup build — Pegium parallelizes those builds across all cores.

Each table reports the full build time and the throughput (MiB/s) for both
engines, plus the Langium-over-Pegium speedup; the workspace tables also report
the peak resident memory (RSS) of each build. Lower time / higher throughput /
lower memory is better. RSS includes each runtime's baseline — Node/V8 carries a
fixed multi-tens-of-MiB heap Pegium's native process does not — so read it
alongside how it grows with input size. Langium 4.3.0 / Node.js 26.

Single-file full build (64 KiB):

| language | pegium | pegium MiB/s | langium | langium MiB/s | speedup |
| --- | ---: | ---: | ---: | ---: | ---: |
| arithmetics | 3.28 ms | 19.1 | 272 ms | 0.2 | ~83× |
| domainmodel | 1.12 ms | 55.8 | 151 ms | 0.4 | ~134× |
| requirements | 1.00 ms | 62.6 | 39 ms | 1.6 | ~39× |
| statemachine | 1.29 ms | 48.5 | 78 ms | 0.8 | ~61× |

Workspace full build, ~250 KB (all files built simultaneously at startup):

| language | pegium | pegium MiB/s | langium | langium MiB/s | speedup | pegium RSS | langium RSS | RSS ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| arithmetics | 1.66 ms | 150.8 | 491 ms | 0.5 | ~296× | 18 MiB | 209 MiB | ~12× |
| domainmodel | 1.12 ms | 223.9 | 230 ms | 1.1 | ~206× | 18 MiB | 185 MiB | ~10× |
| statemachine | 1.18 ms | 214.9 | 209 ms | 1.2 | ~177× | 18 MiB | 186 MiB | ~10× |
| requirements | 2.21 ms | 113.6 | 133 ms | 1.9 | ~60× | 18 MiB | 174 MiB | ~10× |

Workspace full build, ~12 MB (all files built simultaneously at startup):

| language | pegium | pegium MiB/s | langium | langium MiB/s | speedup | pegium RSS | langium RSS | RSS ratio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| arithmetics | 48 ms | 247.9 | 24.3 s | 0.5 | ~502× | 602 MiB | 3.1 GiB | ~5× |
| domainmodel | 33 ms | 365.0 | 10.1 s | 1.2 | ~308× | 346 MiB | 2.1 GiB | ~6× |
| statemachine | 38 ms | 312.4 | 10.2 s | 1.2 | ~265× | 390 MiB | 2.2 GiB | ~6× |
| requirements | 103 ms | 116.5 | 51.8 s | 0.2 | ~503× | 328 MiB | 1.8 GiB | ~6× |

Numbers are indicative and hardware-dependent; reproduce them on your own machine
with:

```bash
cmake --build build -j --target PegiumBench
# Uses a sibling ../langium checkout; pass --setup to clone + build the latest
# Langium and install the bench harness automatically.
python3 tools/compare_langium_bench.py --setup
```

`PegiumBench` (under [`tests/bench/`](tests/bench/)) and the Langium harness
([`tools/langium-bench/bench-examples.mjs`](tools/langium-bench/bench-examples.mjs))
generate the same inputs and report the same format, which
[`tools/compare_langium_bench.py`](tools/compare_langium_bench.py) diffs into the
tables above.

## License

Pegium is [MIT licensed](LICENSE) (c) 2024-2026 Yannick Daveluy.
