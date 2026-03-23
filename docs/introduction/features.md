# Features

Designing a language is hard, whether you are building a small DSL or a larger
programming-language-like system. You have to parse source text, produce a
semantic model, resolve references, manage multi-file workspaces, and provide a
good editing experience.

Pegium exists to remove that framework work so you can focus on the semantics
of your language.

In this chapter, you will get a closer look at the main areas Pegium covers:

- grammar and parsing
- semantic models
- cross-references and linking
- workspace management
- editing support

## Grammar and parsing

Pegium grammars are written directly in C++ through `PegiumParser`
subclasses. Parser expressions cover terminals, named rules, repetitions,
lookahead, skippers, and infix parsing. The parser runtime already includes
expectation tracking and recovery-aware behavior, so parsing is not just a
one-shot syntax check.

## Semantic models

Pegium does not stop at producing syntax trees. AST nodes are ordinary C++20
types, which means the semantic model of your language stays directly visible
in your codebase. At the same time, the CST remains available for source-aware
tasks such as formatting, comment handling, and precise editor selections.

This makes it practical to keep semantic logic, tooling, and source mapping in
one coherent runtime model.

## Cross-references and linking

Real languages need names to resolve to declarations. Pegium models references
explicitly through the reference pipeline: name computation, scope computation,
scope providers, linking, and workspace indexing. Once this is wired, the same
information can be reused for linking, completion, rename, and navigation.

## Workspace management

Most languages quickly become multi-file languages. Pegium includes shared
workspace services that keep track of documents, indexing, and rebuilds across
an entire project. This allows references and diagnostics to work across files
instead of being limited to one isolated parse result.

## Editing support

Pegium is also built for editor integration. Language servers sit on top of the
same document and semantic model, and the default service set already covers a
broad set of LSP features such as completion, hover, definition, references,
rename, document symbols, and code actions.

Formatter, validation, completion, hover, and other features remain explicit
services, so you can keep the defaults where they fit and replace only the
parts that are specific to your language.

## Try it out

If you want to see these features working together, continue with
[showcases](showcases.md) or the shipped [examples](../examples/index.md).

If you want to start building your own language, the next stop is the
[workflow](../learn/workflow/index.md).
