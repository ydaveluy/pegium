# Features

Designing a language is hard. You have to parse source text, produce a semantic model, resolve references, manage multi-file workspaces, and provide a good editing experience. Pegium handles that framework work so you can focus on the semantics of your language.

This chapter covers the main areas Pegium addresses:

- grammar and parsing
- semantic models
- cross-references and linking
- workspace management
- editing support

## Grammar and parsing

You write grammars directly in C++ through `PegiumParser` subclasses. Parser expressions cover terminals, named rules, repetitions, lookahead, skippers, and infix parsing. The runtime includes expectation tracking and recovery-aware behavior, so parsing is more than a one-shot syntax check.

## Semantic models

Pegium does not stop at syntax trees. AST nodes are ordinary C++20 types, so the semantic model of your language stays directly visible in your codebase. The CST remains available for source-aware tasks such as formatting, comment handling, and precise editor selections. Semantic logic, tooling, and source mapping live in one runtime model.

## Cross-references and linking

Real languages need names to resolve to declarations. Pegium models references explicitly through the reference pipeline: name computation, scope computation, scope providers, linking, and workspace indexing. Once wired, the same information drives linking, completion, rename, and navigation.

## Workspace management

Most languages quickly become multi-file. Pegium includes shared workspace services that track documents, indexing, and rebuilds across a project. References and diagnostics work across files instead of one isolated parse result.

## Editing support

Language servers sit on top of the same document and semantic model. The default service set already covers a broad range of LSP features: completion, hover, definition, references, rename, document symbols, and code actions.

Formatter, validation, completion, hover, and other features remain explicit services. Keep the defaults where they fit and replace only the parts specific to your language.

## Related pages

- [Showcases](showcases.md)
- [Examples](../examples/index.md)
- [End-to-end walkthrough](../learn/walkthrough.md)
