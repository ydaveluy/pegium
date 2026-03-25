# Grammar Reference

This page summarizes the user-facing grammar surface of Pegium.

If you want the guided explanation first, start with
[3. Write the Grammar](../learn/workflow/write_grammar.md).

All snippets assume:

```cpp
#include <pegium/core/parser/PegiumParser.hpp>
using namespace pegium::parser;
```

## Parser class

A Pegium grammar starts with a `PegiumParser` subclass:

```cpp
class MyParser : public PegiumParser {
public:
  using PegiumParser::PegiumParser;
  using PegiumParser::parse;

protected:
  const pegium::grammar::ParserRule &getEntryRule() const noexcept override {
    return Module;
  }

  const Skipper &getSkipper() const noexcept override {
    return skipper;
  }

  Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
  Rule<ast::Module> Module{"Module", "module"_kw + assign<&ast::Module::name>(ID)};
};
```

Inside that class, prefer the user-facing aliases:

- `Terminal<T>`
- `Rule<T>`
- `Infix<T, Left, Op, Right>`

`Rule<T>` becomes a parser rule when `T` derives from `AstNode`, and a
data-type rule otherwise.

## Matching primitives

The common low-level building blocks are:

- `"_kw"` for literals such as keywords, punctuation, and operators
- `"_cr"` for character ranges
- `dot` for any single character

Examples:

```cpp
"entity"_kw.i()
"{"_kw
"a-zA-Z_"_cr
"0-9"_cr
dot
```

Use `.i()` on a literal when the keyword should be case-insensitive.

## Terminals and rules

Use `Terminal<T>` for lexical items:

```cpp
Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};
```

Use `Rule<T>` for parser-level constructs:

```cpp
Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID)};
```

The important behavior difference is:

- terminals are contiguous
- rules use the current skipper

## Main grammar operators

Pegium uses a PEG-style expression language.

- sequence: `+`
- ordered choice: `|`
- unordered group: `&`
- optional: `option(...)`
- zero or many: `many(...)`
- one or many: `some(...)`
- bounded repetition: `repeat<...>(...)`
- positive lookahead: unary `&`
- negative lookahead: unary `!`
- non-greedy range: `<=>`

Examples:

```cpp
some(ID, "."_kw)
"+"_kw | "-"_kw
option("extends"_kw + assign<&ast::Entity::superType>(QualifiedName))
"/*"_kw <=> "*/"_kw
```

## Assignments and actions

Assignments are how the grammar fills the AST:

- `assign<&T::member>(...)`
- `append<&T::vectorMember>(...)`
- `enable_if<&T::flag>(...)`

Actions are how the grammar makes the tree shape more explicit:

- `create<T>()`
- `action<T>()`
- `nest<&T::member>()`

Example:

```cpp
Rule<ast::Feature> FeatureRule{
    "Feature",
    option(enable_if<&ast::Feature::many>("many"_kw.i())) +
        assign<&ast::Feature::name>(ID) + ":"_kw +
        assign<&ast::Feature::type>(QualifiedName)};
```

## Infix rules

Use `Infix<...>` for operator precedence and associativity:

```cpp
Infix<ast::BinaryExpression, &ast::BinaryExpression::left,
      &ast::BinaryExpression::op, &ast::BinaryExpression::right>
    BinaryExpression{"BinaryExpression",
                     PrimaryExpression,
                     LeftAssociation("*"_kw | "/"_kw),
                     LeftAssociation("+"_kw | "-"_kw)};
```

Operators are declared from highest precedence to lowest precedence. Use
`LeftAssociation(...)` or `RightAssociation(...)` depending on the grouping you
want.

## Skippers

The skipper decides what may appear automatically between parser elements.

Typical setup:

```cpp
static constexpr auto WS = some(s);
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

Skipper skipper = skip(ignored(WS), hidden(ML_COMMENT, SL_COMMENT));
```

- `ignored(...)` removes tokens entirely from the CST
- `hidden(...)` preserves tokens as hidden CST nodes

You can also apply a local skipper to a rule or expression with `.skip(...)`
when one part of the grammar needs different whitespace behavior.

## Choosing the right abstraction

When in doubt:

- use a terminal when the text must be contiguous
- use a rule when the construct is part of the visible language structure
- use assignments to make the AST shape obvious
- use `Infix` only when precedence is the real problem

## Related pages

- [Grammar Essentials](../build-a-language/grammar.md)
- [Semantic Model](semantic-model.md)
- [Formatter DSL](formatter-dsl.md)
