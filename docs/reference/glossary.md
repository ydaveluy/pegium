# Glossary

Short definitions of the Pegium terms used across the docs. For deeper coverage, follow the links to the workflow and subsystem references.

## AST

The semantic tree built by parser rules and assignments. AST nodes are regular C++ types derived from `pegium::AstNode`.

## CST

The concrete syntax tree that preserves source structure and hidden nodes. Use it for formatting and source-aware editor features.

## `PegiumParser`

The user-facing parser base class for defining Pegium grammars in C++.

## `Terminal<T>`

A named lexical rule that matches contiguous text.

## `Rule<T>`

A named non-terminal rule. When `T` is an AST type it becomes a parser rule; otherwise it becomes a data-type rule.

## Skipper

The rule set that handles ignored and hidden tokens between parser elements, such as whitespace and comments.

## Reference

A source-level name stored in the AST that may later resolve to a target node.

## Scope

The set of visible symbols available at one reference site.

## Linking

Resolving a written reference to a concrete AST target.

## Services

The language-specific objects that hold parser, references, validation, workspace, and LSP providers. `CoreServices` covers the semantic layer; `Services` (the LSP variant) extends it with editor-facing providers.

## SharedServices

The runtime services reused by every registered language: workspace manager, document builder, index manager, and the LSP runtime when present.

## Module

A function (often template-based, e.g. `installArithmeticsCoreModule(...)`) that installs language-specific defaults onto a service container. The shipped examples follow the convention `install<Language>CoreModule` and `install<Language>LspModule`.

## Document lifecycle

The staged pipeline that moves a document from changed text to parsed, linked, indexed, and validated state. See [Document Lifecycle](document-lifecycle.md).

## ParseResult

The output of one parser invocation. Contains the AST root, the CST root, the parse diagnostics, and recovery information when the input was not syntactically valid.

## AstNodeDescription

A workspace-level description of one declaration: its name, kind, source range, and originating document. The index manager exposes descriptions for cross-document lookups; scope computation produces them.

## LocalSymbols

The per-document container of local declarations, bucketed by enclosing container. `DefaultScopeProvider` reads it during `visitScopeEntries(...)` to walk visibility from the reference site outward.

## DocumentBuilder

The orchestrator that drives a document through the lifecycle states and emits update events when documents change or are deleted. Caches that must invalidate on text change subscribe to `DocumentBuilder::onUpdate`.

## Related pages

- [Reference](index.md)
- [Document Lifecycle](document-lifecycle.md)
- [Semantic Model](semantic-model.md)
