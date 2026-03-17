# Workspace Concepts

Pegium's workspace layer is the backbone for multi-document language features.

## Main types

- `workspace::Document`
- `workspace::TextDocument`
- `workspace::Documents`
- `workspace::DocumentBuilder`
- `workspace::IndexManager`
- `workspace::WorkspaceManager`

`workspace::Document` is the main per-file object. It keeps:

- text content
- parse result
- AST/CST
- references
- diagnostics
- analysis state

## Responsibilities

- keep text documents and parsed documents synchronized
- rebuild syntax trees on change
- maintain exported symbols and indexes
- support cross-document language features such as references and rename

## Document states

Documents move through states such as `Changed`, `Parsed`, `Linked`, and
`Validated`. This staged model lets services depend on stable intermediate
results instead of recomputing everything ad hoc.

## Practical guidance

- keep document parsing deterministic
- avoid custom workspace behavior until your language actually needs it
- customize scoping or indexing first; customize the workspace infrastructure
  itself only when the default lifecycle is too restrictive
