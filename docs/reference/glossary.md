# Glossary

This page collects the Pegium terms that appear throughout the documentation.

Use it as a short-definition page, not as a replacement for the workflow or the
subsystem references.

## AST

The semantic tree built by parser rules and assignments. AST nodes are regular
C++ types derived from `pegium::AstNode`.

## CST

The concrete syntax tree that preserves source structure and hidden nodes. The
CST is especially useful for formatting and source-aware editor features.

## `PegiumParser`

The user-facing parser base class used to define Pegium grammars in C++.

## `Terminal<T>`

A named lexical rule that matches contiguous text.

## `Rule<T>`

A named non-terminal rule. When `T` is an AST type it becomes a parser rule;
otherwise it becomes a data-type rule.

## Skipper

The rule set that handles ignored and hidden tokens such as whitespace and
comments between parser elements.

## Reference

A source-level name stored in the AST that may later resolve to a target node.

## Scope

The set of visible symbols available at one reference site.

## Linking

The process of resolving a written reference to a concrete AST target.

## Services

The language-specific objects that hold parser, references, validation,
workspace, and LSP providers.

## Document lifecycle

The staged pipeline through which a document moves from changed text to parsed,
linked, indexed, and validated state.

## Continue with

- [Start Here](start-here.md)
- [Document Lifecycle](document-lifecycle.md)
- [Semantic Model](semantic-model.md)
