# FAQ and Common Pitfalls

Short answers to the questions and silent traps that come up most when building a Pegium language. Several pitfalls compile cleanly but misbehave at runtime, so know them before you hit them.

## Common pitfalls

**A named declaration must derive `pegium::NamedAstNode`.** Deriving plain
`pegium::AstNode` and adding your own `string name` compiles, but the default
`NameProvider` reads `NamedAstNode::name`, so the node is never named, exported,
or linked. Use `NamedAstNode` for anything with a name.

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
first access. Check `has_error_diagnostics` (or the reference's `bool` conversion)
before following it.

## Questions

**Does Pegium generate code from a grammar file?** No. The grammar, AST, and
services are hand-written C++. There is no external grammar DSL or scaffolding step
(the optional project template just renames a starter project).

**Do I need to clone Pegium to build my language?** No — pull it in via
`FetchContent` (see [Create a Project](learn/workflow/scaffold.md)). Clone it
only to run the examples or to contribute.

**How do I test my language?** Build a document with the `pegium::test::*` helpers
and assert on the AST and diagnostics — see
[Test Your Language](learn/test-your-language.md).

**How do I run my language without an editor?** Parse a file and walk the AST
headlessly — see [Run a Language Headlessly](learn/consume-the-ast.md).

**How do I hook it to an editor?** Start the server with `runLanguageServerMain`
and connect a thin client — see
[Integrate with an Editor](build-a-language/editor-integration.md).

## Related pages

- [Build a Language End-to-End](learn/walkthrough.md)
- [Debugging a Grammar](build-a-language/debugging-grammar.md)
