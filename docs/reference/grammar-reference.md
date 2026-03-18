# Grammar Reference

This page is the canonical grammar reference for Pegium.

Use this page when you need the exact parser DSL surface. If you want the
recommended learning order first, go back to
[Write the Grammar](../learn/workflow/write_grammar.md).

All snippets assume:

```cpp
#include <pegium/parser/PegiumParser.hpp>
using namespace pegium::parser;
```

Snippets that use `Terminal<T>`, `Rule<T>`, or `Infix<...>` assume they are
written inside a subclass of `pegium::parser::PegiumParser`.

## Parser class

A Pegium grammar starts with a parser class deriving from
`pegium::parser::PegiumParser`.

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

## Parser aliases

Inside a `PegiumParser` subclass, prefer these aliases:

- `Terminal<T>`: terminal rule
- `Rule<T>`: data type rule or parser rule, depending on `T`
- `Infix<T, Left, Op, Right>`: infix rule

`Rule<T>` resolves automatically:

- `Rule<ast::Node>` becomes a parser rule
- `Rule<std::string>` or `Rule<double>` becomes a data type rule

## Entry rule

Every parser must override `getEntryRule()`.

The entry rule:

- is the rule used by `PegiumParser::parse(...)`
- must be AST-producing
- is therefore typically a `Rule<RootAstNode>`

Use `getSkipper()` to provide the hidden-token policy for the whole parser.

## Terminals

### `Literal`

Create literals with `"_kw"`:

```cpp
auto kw = "catalogue"_kw;
auto ciKw = "catalogue"_kw.i();
```

`Literal` matches a fixed piece of text.

Use it for:

- keywords such as `"entity"_kw`
- punctuation such as `"{"_kw`, `":"_kw`, or `";"_kw`
- fixed operators such as `"+"_kw` or `"->"_kw`

Important behavior:

- literals are case-sensitive by default
- call `.i()` to make a literal case-insensitive
- when the literal ends with a word character, Pegium enforces a word boundary

That last point means `"entity"_kw` matches `entity`, but not the `entity`
prefix inside `entityName`.

Case-insensitive literals are useful for language keywords:

```cpp
"module"_kw.i()
"entity"_kw.i()
"extends"_kw.i()
```

For punctuation and operators, the default case-sensitive form is usually the
right one.

### `CharacterRange`

Create ranges with `"_cr"`:

```cpp
auto lower = "a-z"_cr;
auto digit = "0-9"_cr;
auto notNewline = "^\n"_cr;
```

`CharacterRange` matches one character chosen from a set or interval.

Typical uses:

- letters: `"a-zA-Z"_cr`
- digits: `"0-9"_cr`
- identifier start: `"a-zA-Z_"_cr`
- identifier continuation: `"a-zA-Z0-9_"_cr`

Rules of thumb:

- `a-z` means every character from `a` to `z`
- you can concatenate several ranges in the same expression
- a leading `^` negates the range

Examples:

```cpp
auto identifierStart = "a-zA-Z_"_cr;
auto identifierPart = "a-zA-Z0-9_"_cr;
auto hexDigit = "0-9a-fA-F"_cr;
auto notQuote = "^\""_cr;
```

`CharacterRange` is best for compact lexical constraints. When the language
needs to match structured text rather than one character at a time, combine
several ranges with parser expressions or move to a named rule.

### `AnyCharacter`

Use `dot`:

```cpp
auto any = dot;
```

`dot` matches any single character except end-of-input.

In practice it is useful for:

- fallback matching
- comment bodies
- scanning until a delimiter
- simple “consume one more character” patterns

Example:

```cpp
auto blockComment = "/*"_kw <=> "*/"_kw;
auto untilEndOfLine = many(!eol + dot);
```

`dot` is the most permissive primitive. Prefer `Literal` or `CharacterRange`
when the intent is more specific.

### `Terminal<T>`

Use terminals for lexical items:

```cpp
// inside a Parser subclass
Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};
```

Important: a terminal is contiguous. Hidden tokens cannot appear between the
elements inside a terminal rule.

If you need whitespace or comments between parts, use `Rule<T>` instead.

This distinction is easy to miss:

```cpp
Terminal<std::string> A{"A", ID + "."_kw + ID};
Rule<std::string> B{"B", ID + "."_kw + ID};
```

- `A` only matches contiguous text such as `foo.bar`
- `B` can match `foo.bar`, `foo . bar`, or `foo /*comment*/ . bar`, depending
  on the skipper

## Rules

### `Rule<T>`

Use `Rule<T>` for named parser-level constructs:

```cpp
// inside a Parser subclass
Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID)};
```

Rules run with the current skipper, so hidden tokens can appear between their
elements.

### Data-type rules

`Rule<T>` is a data-type rule when `T` is not derived from `AstNode`.

Typical use cases:

- qualified names
- scalar values
- enums
- strongly-typed terminal conversions

### Parser rules

`Rule<T>` is a parser rule when `T` derives from `AstNode`.

Use parser rules for:

- AST-producing language constructs
- nesting and containment
- assignments and actions

## PEG combinators

### `Group`

Sequence with `+`:

```cpp
auto qualifiedName = some(w) + "."_kw + some(w);
```

### `OrderedChoice`

Choice with `|`:

```cpp
auto sign = "+"_kw | "-"_kw;
```

### `UnorderedGroup`

All elements once, in any order, with `&`:

```cpp
auto modifiers = "public"_kw & "static"_kw;
```

### `Repetition`

Helpers:

```cpp
auto optionalSemicolon = option(";"_kw);          // 0..1
auto spaces = many(s);                            // 0..N
auto identifierTail = some(w | d | "_"_kw);       // 1..N
auto exactly2Digits = repeat<2>(d);               // exactly 2
auto oneToThreeDigits = repeat<1, 3>(d);          // min/max
auto csvWords = many(some(w), ","_kw);            // separated repetition
```

### `AndPredicate`

Lookahead with unary `&`:

```cpp
auto beforeSemicolon = &";"_kw;
```

`AndPredicate` checks that the next expression matches without consuming input.
Use it for lookahead constraints.

### `NotPredicate`

Negative lookahead with unary `!`:

```cpp
auto untilEolChar = !eol + dot;
```

`NotPredicate` checks that the next expression does not match, again without
consuming input. It is often combined with `dot` to express “consume until ...”.

### Until operator (`<=>`)

`start <=> end` parses from `start` to the first `end`:

```cpp
auto blockComment = "/*"_kw <=> "*/"_kw;
```

This operator is a concise non-greedy “from ... to ...” pattern. It is
especially useful for comment terminals.

## Assignments and actions

### `Assignment`

Use `assign`, `append`, and `enable_if` to fill AST members:

```cpp
// inside a Parser subclass
struct FieldNode : pegium::AstNode {
  std::string name;
  std::vector<std::string> tags;
  bool optional = false;
};

Rule<FieldNode> FieldRule{
    "Field",
    assign<&FieldNode::name>("name"_kw) +
        ":"_kw +
        many(append<&FieldNode::tags>("tag"_kw), ","_kw) +
        enable_if<&FieldNode::optional>("?"_kw)};
```

### `Action`

Use `create<T>()`, `action<T>()`, and `nest<&T::member>()` to create or wrap
AST nodes while parsing.

```cpp
struct Expr : pegium::AstNode {};
struct NumberExpr : Expr {
  std::string value;
};
struct UnaryExpr : Expr {
  pegium::pointer<Expr> operand;
};

auto newNumber = action<NumberExpr>();
auto wrapCurrent = action<UnaryExpr, &UnaryExpr::operand>();
```

## Infix rules

Use `Infix<...>` for operator-heavy languages with precedence and
associativity.

```cpp
// inside a Parser subclass
Infix<ast::BinaryExpression, &ast::BinaryExpression::left,
      &ast::BinaryExpression::op, &ast::BinaryExpression::right>
    BinaryExpression{"BinaryExpression",
                     PrimaryExpression,
                     LeftAssociation("%"_kw),
                     LeftAssociation("^"_kw),
                     LeftAssociation("*"_kw | "/"_kw),
                     LeftAssociation("+"_kw | "-"_kw)};
```

Use:

- `LeftAssociation(...)` for left-associative operators
- `RightAssociation(...)` for right-associative operators

Important: the operator declarations are listed from the **strongest
precedence** to the **weakest precedence**.

So this declaration:

```cpp
Infix<ast::BinaryExpression, &ast::BinaryExpression::left,
      &ast::BinaryExpression::op, &ast::BinaryExpression::right>
    BinaryExpression{"BinaryExpression",
                     PrimaryExpression,
                     LeftAssociation("%"_kw),
                     LeftAssociation("^"_kw),
                     LeftAssociation("*"_kw | "/"_kw),
                     LeftAssociation("+"_kw | "-"_kw)};
```

means:

- `%` binds more strongly than `^`
- `^` binds more strongly than `*` and `/`
- `*` and `/` bind more strongly than `+` and `-`

That precedence table drives the AST grouping:

- `2 + 3 * 4` parses as `2 + (3 * 4)`
- `a * b + c` parses as `(a * b) + c`
- `a - b - c` with `LeftAssociation(...)` parses as `(a - b) - c`
- `a ^ b ^ c` with `RightAssociation(...)` parses as `a ^ (b ^ c)`

The first argument after the rule name is the base rule. In practice this is
usually a rule such as `PrimaryExpression` containing the atomic expressions of
the language:

- number or string literals
- identifiers
- grouped expressions
- function calls

`Infix` then builds binary-expression nodes on top of that base rule according
to the precedence and associativity declarations.

This is especially useful when you want to:

- avoid manually writing one rule per precedence level
- keep a uniform AST node type for binary expressions
- make the operator table obvious in one place

An infix rule cannot be used as a terminal. It is a parser-level construct.

## Skipper integration

### Global skipper

Build a skipper with ignored and hidden terminals:

```cpp
// inside a Parser subclass
static constexpr auto WS = some(s);
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

Skipper skipper =
    SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
```

The skipper defines which tokens may appear automatically between parser-level
elements, and whether those tokens are discarded or preserved in the CST.

### `ignore(...)`

`ignore(...)` is for tokens that should disappear completely after parsing.

Typical example:

```cpp
static constexpr auto WS = some(s);
Skipper skipper = SkipperBuilder().ignore(WS).build();
```

Ignored tokens:

- are consumed automatically between elements of parser rules and data-type
  rules
- do not appear in the CST
- cannot be visited later as CST nodes

### `hide(...)`

`hide(...)` is for tokens that should not influence parsing, but should remain
available in the CST as hidden nodes.

Typical example:

```cpp
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
Terminal<> ML_COMMENT{"ML_COMMENT", "/*"_kw <=> "*/"_kw};

Skipper skipper =
    SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
```

Hidden tokens:

- are consumed automatically between elements of parser rules and data-type
  rules
- do appear in the CST
- are marked as hidden nodes
- can be used later by formatting or source-aware features

### Parser rules and data-type rules

The skipper applies between the elements of parser-level rules.

That includes:

- `Rule<T>` when `T` is an AST node type
- `Rule<T>` when `T` is a scalar or other non-AST value type

So a rule such as:

```cpp
// inside a Parser subclass
Rule<std::string> QualifiedName{"QualifiedName", ID + "."_kw + ID};
```

can accept all of the following depending on the configured skipper:

- `foo.bar`
- `foo . bar`
- `foo /* comment */ . bar`

### Terminals

The skipper does **not** apply inside `Terminal<T>`.

So:

```cpp
// inside a Parser subclass
Terminal<std::string> QualifiedNameToken{"QualifiedNameToken",
                                         ID + "."_kw + ID};
```

matches contiguous text only. It does not allow ignored or hidden tokens
between `ID`, `"."`, and `ID`.

This is why whitespace-sensitive lexical constructs usually belong in
`Terminal<T>`, while whitespace-tolerant structured constructs usually belong in
`Rule<T>`.

### Local skipper on expressions

Supported on `Group`, `OrderedChoice`, `UnorderedGroup`, and `Repetition`:

```cpp
auto localSkipper = SkipperBuilder().ignore(WS).build();

auto grouped = ("a"_kw + "b"_kw).with_skipper(localSkipper);
auto choice = ("a"_kw | "b"_kw).with_skipper(localSkipper);
auto unordered = ("a"_kw & "b"_kw).with_skipper(localSkipper);
auto repeated = some("a"_kw).with_skipper(localSkipper);
```

### Local skipper on rules

Use `opt::with_skipper(...)` for named rules:

```cpp
// inside a Parser subclass
Rule<std::string> Token{
    "Token", "a"_kw + "b"_kw, opt::with_skipper(localSkipper)};
```

## Built-in shortcuts

Predefined expressions in `pegium::parser`:

```cpp
auto end = eof;
auto endLine = eol;
auto space = s;
auto nonSpace = S;
auto word = w;
auto nonWord = W;
auto digit = d;
auto nonDigit = D;
```

These shortcuts are meant to cover the most common lexical building blocks:

- `eof`: end of input
- `eol`: line ending (`\n`, `\r\n`, or `\r`)
- `s`: whitespace characters
- `S`: non-whitespace character
- `w`: word character (`[a-zA-Z0-9_]`)
- `W`: non-word character
- `d`: digit (`[0-9]`)
- `D`: non-digit character

Examples:

```cpp
Terminal<> WS{"WS", some(s)};
Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
Terminal<> SL_COMMENT{"SL_COMMENT", "//"_kw <=> &(eol | eof)};
```

## Nullability constraints

Several APIs intentionally reject nullable expressions:

- `Terminal<T>`
- `Rule<T>`
- `assign`, `append`, `enable_if`
- `many`, `some`, `repeat`

## Related pages

- [Write the Grammar](../learn/workflow/write_grammar.md)
- [Parser Contracts](parser-contracts.md)
- [Glossary](glossary.md)
