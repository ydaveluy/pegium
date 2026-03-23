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

## Get Started

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

## License

Pegium is [MIT licensed](LICENSE) (c) 2024-2026 Yannick Daveluy.
