# Grammar Essentials

This page is about the practical side of writing a Pegium grammar.

If you are learning Pegium step by step, start with
[3. Write the Grammar](../learn/workflow/write_grammar.md) first. Come back to
this page when you want a subsystem-oriented explanation.

## Start with one parser class

Pegium grammars live in a `PegiumParser` subclass. That class defines:

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

The entry rule should produce the root AST node of the document. In practice,
that means `getEntryRule()` normally returns a `Rule<YourRootAstType>`.

## Think in terms of terminals and rules

The most important design choice in a Pegium grammar is deciding whether a
construct is lexical or structural.

Use `Terminal<T>` when the text must stay contiguous:

- identifiers
- numbers
- operators
- comment tokens

Use `Rule<T>` when the construct is part of the real language structure:

- declarations
- blocks
- expressions
- qualified names that should allow hidden tokens between parts

That distinction has one practical consequence: rules use the current skipper,
terminals do not.

## Use grammar assignments to shape the model

Pegium does not generate a separate semantic model from another language. The
grammar fills the C++ AST types you already defined.

The common building blocks are:

- `assign<&T::member>(...)` for one value
- `append<&T::vectorMember>(...)` for repeated values or children
- `enable_if<&T::flag>(...)` for booleans driven by syntax
- `create<T>()`, `action<T>()`, and `nest<&T::member>()` when the tree needs a
  more explicit shape

This is the point where grammar design and language design meet. If the
resulting AST feels awkward to validate or traverse, the grammar usually needs
another pass too.

## Keep whitespace and comments deliberate

The skipper decides what may appear automatically between parser elements.

Typical setup:

```cpp
static constexpr auto WS = some(s);
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));
```

Use `ignored(...)` for text that should disappear from the CST entirely, and
`hidden(...)` for text that should remain available to source-aware features
such as formatting or hover.

## Reach for `Infix` only when precedence matters

Expression-heavy languages often need operator precedence and associativity.
That is what `Infix<...>` is for.

Use it when:

- the language has several operator levels
- the AST should keep one uniform binary-expression shape
- a precedence table is easier to maintain than a stack of manual rules

If the language only has one or two operator forms, a small explicit rule stack
is often easier to read.

## A good first iteration

A small, stable first grammar is usually better than an ambitious one.

1. Define whitespace, comments, and the core terminals.
2. Add one root rule and one or two declaration rules.
3. Make sure the AST shape is already usable.
4. Add references, precedence, and local skipper tricks only once the basics
   are stable.

## Related pages

- [3. Write the Grammar](../learn/workflow/write_grammar.md)
- [Grammar Reference](../reference/grammar-reference.md)
- [AST and CST](ast-and-cst.md)
