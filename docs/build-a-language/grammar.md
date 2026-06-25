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

## Combinators

A grammar body is an expression built from these combinators.

Matching primitives:

| Combinator | Meaning |
| --- | --- |
| `"…"_kw` | a literal: keyword, punctuation, or operator. Add `.i()` for case-insensitive, `.doc("…")` for hover text |
| `"…"_cr` | a character range/class, e.g. `"a-zA-Z0-9_"_cr`. A leading `^` negates it (`"^0-9"_cr`) |
| `dot` | any single UTF-8 code point, **including newline** (like regex `.` with the DOTALL flag) |
| `s` / `S` | whitespace / non-whitespace (`\s` / `\S`) |
| `w` / `W` | word / non-word character (`\w` / `\W`) |
| `d` / `D` | digit / non-digit (`\d` / `\D`) |
| `eol` / `eof` | end of line / end of input |

Sequencing and choice:

| Combinator | Meaning |
| --- | --- |
| `a + b` | sequence: `a` then `b` |
| `a \| b` | ordered choice: the first matching alternative wins, with no backtracking once one matches — put specific cases first |
| `a & b` | unordered group: `a` and `b` in any order |

Repetition:

| Combinator | Meaning |
| --- | --- |
| `option(a)` | zero or one |
| `many(a)` | zero or more |
| `some(a)` | one or more |
| `many(a, sep)` / `some(a, sep)` | repetition separated by `sep`, e.g. `some(ID, "."_kw)` |
| `repeat<N>(a)` | exactly `N` times |
| `repeat<Min, Max>(a)` | between `Min` and `Max` times (inclusive) |

Lookahead and until:

| Combinator | Meaning |
| --- | --- |
| `&a` | positive lookahead: require `a` ahead without consuming it |
| `!a` | negative lookahead: succeed only if `a` does not match (also negates a class, as in `!s + dot`) |
| `a <=> b` | non-greedy until: `a`, then everything up to and including the first `b` (used for block comments) |

They compose freely:

```cpp
"a-zA-Z_"_cr + many(w)                  // an identifier terminal body
some(ID, "."_kw)                        // a dotted qualified name
option("extends"_kw + assign<&ast::Entity::superType>(QualifiedName))
"/*"_kw <=> "*/"_kw                     // a block comment
```

Matching text is not the same as building the AST — turn matched input into AST
data with the assignment combinators below.

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

## Extend a base grammar with `super()`

A parser is an ordinary C++ class, so one language can inherit another: derive from the base parser to reuse its rules, terminals, and skipper, then change only what differs.

Every rule exposes `super()`, which returns its current definition. Reassigning a rule **with** `super()` *extends* it (the previous body is folded in); reassigning **without** `super()` *replaces* it.

```cpp
class BaseParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override { return Model; }
  const Skipper &getSkipper() const noexcept override { return skipper; }

  Skipper skipper = skip(ignored(some(s)));
  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<ast::Statement> Statement{"Statement", "let"_kw + assign<&ast::Statement::name>(ID)};
  Rule<ast::Model> Model{"Model", some(append<&ast::Model::statements>(Statement))};
};

class ExtendedParser : public BaseParser {
public:
  ExtendedParser() {
    // Add an alternative to the inherited rule. `Model` keeps matching it
    // because rules reference each other by address.
    Statement = Statement.super() | ("const"_kw + assign<&ast::Statement::name>(ID));
  }
};
```

Reassign rules in the derived constructor, after the base has initialized them. Because rules reference one another by address, extending an inherited rule in place is seen by every rule that uses it — including the entry rule. To override a rule completely, reassign it without `super()`:

```cpp
Statement = "fn"_kw + assign<&ast::Statement::name>(ID);  // replaces the base body
```

`super()` works the same on `Rule<T>`, `Terminal<T>`, and data-type rules. Override `getEntryRule()` / `getSkipper()` in the derived class when the entry point or whitespace handling also changes.

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
- [Define the AST](ast-and-cst.md)
