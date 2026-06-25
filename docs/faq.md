# FAQ and Common Pitfalls

Short answers to the questions and silent traps that come up most when building a Pegium language. Several pitfalls compile cleanly but misbehave at runtime, so know them before you hit them.

## Common pitfalls

**Named declarations must derive `pegium::NamedAstNode`.**
The default `NameProvider` names only `NamedAstNode`-derived nodes — it reads
`NamedAstNode::name`. A plain `pegium::AstNode` is unnamed even if its grammar
assigns a `name` field: it is not exported, scoped, or linked by name. Derive
`NamedAstNode` for anything with a name; to name a type that cannot derive it,
override the `NameProvider` service.

**Pass your own services type to `makeDefaultServices`.**
`makeDefaultServices<MyServices>(shared, id)` returns your container, which can
hold language-specific members. The bare `makeDefaultServices(shared, id)`
defaults to the base `pegium::Services` and cannot hold your additions. Assemble
the language through your `installMyCoreModule` / `installMyLspModule` functions.

**AST node types must be default-constructible.** The arena builds nodes before
populating them, so every AST struct needs a usable default constructor (the
defaults you get from deriving `AstNode` / `NamedAstNode` are fine).

**Matching is not assigning.** A grammar element that matches text does nothing to
the AST unless you wrap it in `assign<&T::field>(...)` (single) or
`append<&T::vec>(...)` (repeated).

**`Infix` is an in-class alias.** Inside a `PegiumParser` subclass, `Infix<...>`
aliases `InfixRule<...>`. Outside the class, use `InfixRule<...>`.

**Choice is ordered.** PEG `|` returns the first matching alternative — order
matters, and there is no backtracking to a later alternative once one matches. Put
specific cases first; use `!lookahead` to disambiguate.

**`reference<T>` resolves lazily.** Dereferencing a reference triggers linking on
first access. Gate the whole result with `has_error_diagnostics(document)`; before
following an individual reference, check its `bool` conversion (false when
unresolved).

## Questions

**Does Pegium generate code from a grammar file?** No — the grammar, AST, and
services are hand-written C++; there is no external grammar DSL or grammar-to-code
generation. There *is* an optional project scaffolder (`pegium-new.cmake` — see
[Create a Project](learn/workflow/scaffold.md)) that generates a complete,
ready-to-build project by substituting your language name, id, and extension into
templated CMake/grammar/CLI/LSP/VS Code files.

**Do I need to clone Pegium to build my language?** No — pull it in via
`FetchContent` (see [Create a Project](learn/workflow/scaffold.md)). Clone it
only to run the examples or to contribute.

**How do I test my language?** Build a document with the `pegium::testing::*` helpers
and assert on the AST and diagnostics — see
[Test Your Language](learn/test-your-language.md).

**How do I run my language without an editor?** Parse a file and walk the AST
headlessly — see [Run a Language Headlessly](learn/consume-the-ast.md).

**How do I hook it to an editor?** Start the server with `runLanguageServerMain`
and connect a thin client — see
[Integrate with VS Code](build-a-language/editor-integration.md).

## Related pages

- [Build a Language End-to-End](learn/walkthrough.md)
- [Debugging a Grammar](build-a-language/debugging-grammar.md)
