# Introduction

Pegium is a C++20 toolkit for building textual languages. It provides the
runtime pieces needed to move from a grammar to an editor experience:

- parser expressions and named rules
- AST and CST construction
- references and scoping
- validation
- formatting
- LSP features and server wiring

Pegium is especially useful when you want the ergonomics of a language
framework, but you want to keep the implementation in modern C++.

## What Pegium gives you

Pegium is not just a parser library. It is a stack of cooperating pieces:

1. grammar definitions written directly in C++
2. AST and CST construction during parsing
3. references and linking across documents
4. semantic validation
5. formatting and source-aware CST utilities
6. default LSP providers and language-server wiring

That combination is what makes it possible to move from a small example parser
to a language with completion, hover, rename, references, and formatting
without inventing each layer from scratch.

## What kind of projects fit well

Pegium works well when:

- the implementation language should stay C++
- the language has structure and editor tooling needs, not just parsing
- the project benefits from AST, scoping, validation, and LSP support living in
  one framework
- examples and explicit extension points are more useful than a highly magical
  code-generation workflow

## How to read the documentation

If you are new to Pegium:

1. read [Why Pegium](why-pegium.md)
2. read [Getting Started](../getting-started/index.md)
3. follow [Build a Language](../build-a-language/index.md)

If you already know Langium:

1. read [Pegium for Langium users](langium-users.md)
2. jump to [Grammar essentials](../build-a-language/grammar.md)
3. then read [References and scoping](../build-a-language/references-and-scoping.md),
   [Validation](../build-a-language/validation.md), and
   [Formatting](../build-a-language/formatting.md)
