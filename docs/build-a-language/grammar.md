# Grammar Essentials

Write a Pegium grammar: how to structure the parser class, choose terminals versus rules, and shape the AST.

To learn Pegium step by step, build a language end-to-end in the [walkthrough](../learn/walkthrough.md) first. Come back here when you want a subsystem-oriented explanation.

## Start with one parser class

Pegium grammars live in a `PegiumParser` subclass. The class defines:

- the entry rule
- the skipper
- the named terminals and rules of the language

```cpp
using namespace pegium::parser;

class DomainModelParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Domainmodel;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

  static constexpr auto WS = some(s);
  Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
  Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};
  Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};
  Rule<ast::DomainModel> Domainmodel{
      "Domainmodel",
      some(append<&ast::DomainModel::elements>(AbstractElementRule))};
};
```

The entry rule produces the root AST node of the document, so `getEntryRule()` normally returns a `Rule<YourRootAstType>`.

## Think in terms of terminals and rules

The key design choice is whether a construct is lexical or structural.

Use `Terminal<T>` when the text must stay contiguous:

- identifiers
- numbers
- operators
- comment tokens

Use `Rule<T>` when the construct is part of the language structure:

- declarations
- blocks
- expressions
- qualified names that should allow hidden tokens between parts

The consequence: rules use the current skipper, terminals do not.

## Use grammar assignments to shape the model

Pegium does not generate a separate semantic model from another language. The grammar fills the C++ AST types you already defined.

The common building blocks are:

- `assign<&T::member>(...)` for one value
- `append<&T::vectorMember>(...)` for repeated values or children
- `enable_if<&T::flag>(...)` for booleans driven by syntax
- `create<T>()` and `nest<&T::member>()` when the tree needs a more explicit
  shape

This is where grammar design and language design meet. If the resulting AST feels awkward to validate or traverse, the grammar usually needs another pass.

## Keep whitespace and comments deliberate

The skipper decides what may appear automatically between parser elements.

Typical setup:

```cpp
static constexpr auto WS = some(s);
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));
```

- Use `ignored(...)` for text that should disappear from the CST entirely.
- Use `hidden(...)` for text that should remain available to source-aware features such as formatting or hover.

## Document keywords for hover

Attach documentation to a keyword with `.doc("…")`; it renders on hover over
that keyword in the editor, just like declaration doc-comments do:

```cpp
Rule<Component> ComponentRule{
    "Component", "class"_kw.doc("A class component declares typed members.") +
                     assign<&Component::name>(ID)};
```

`.doc(...)` leaves the keyword otherwise unchanged, so it composes with `+` and
`|` like any keyword. The text is a non-owning view — pass a string literal (or
a string that outlives the grammar). Hover is served by the
`DocumentationProvider`, so a language can override how the text is rendered.
See [LSP Services](lsp-services.md).

## Reach for `Infix` only when precedence matters

Expression-heavy languages often need operator precedence and associativity. That is what `Infix<...>` is for.

Use it when:

- the language has several operator levels
- the AST should keep one uniform binary-expression shape
- a precedence table is easier to maintain than a stack of manual rules

If the language has only one or two operator forms, a small explicit rule stack is easier to read.

## Practical advice

A small, stable first grammar beats an ambitious one:

1. Define whitespace, comments, and the core terminals.
2. Add one root rule and one or two declaration rules.
3. Make sure the AST shape is already usable.
4. Add references, precedence, and local skipper tricks only once the basics
   are stable.

## Related pages

- [Build a Language End-to-End](../learn/walkthrough.md)
- [Grammar Reference](../reference/grammar-reference.md)
- [AST and CST](ast-and-cst.md)
