# Grammar Essentials

Pegium grammars are defined directly in C++ by subclassing
`pegium::parser::PegiumParser` and declaring named rules as members.

## Start from a parser class

The parser class is the root of the grammar. It provides the rule aliases you
should use in day-to-day Pegium code and defines the entry rule and skipper.

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

## The aliases you should use

Inside a `PegiumParser` subclass, Pegium exposes three aliases:

- `Terminal<T>` for terminal rules
- `Rule<T>` for named non-terminal rules
- `Infix<T, Left, Op, Right>` for precedence-driven infix parsing

Use these aliases instead of spelling `TerminalRule`, `DataTypeRule`,
`ParserRule`, or `InfixRule` directly in user code.

## Entry rule and parser lifecycle

Every parser must override `getEntryRule()`. The returned rule is the root rule
used by `PegiumParser::parse(...)`.

Important detail:

- the entry rule must be AST-producing
- in practice, that means `getEntryRule()` returns a `Rule<AstNodeType>`
- a `Rule<std::string>` or `Rule<double>` is useful inside the grammar, but it
  cannot be the parser entry point

Override `getSkipper()` when your grammar has hidden tokens such as whitespace
or comments.

## Terminal vs `Rule`

This is the most important distinction to keep in mind when designing a Pegium
grammar.

### `Terminal<T>`

Use `Terminal<T>` for lexical items such as identifiers, numbers, or comments.

```cpp
Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
Terminal<double> NUMBER{"NUMBER", some(d) + option("."_kw + many(d))};
```

Terminals are atomic. There cannot be hidden tokens between the internal
elements of a terminal rule.

For example, this terminal only matches contiguous text:

```cpp
Terminal<std::string> DottedNameToken{"DottedNameToken", ID + "."_kw + ID};
```

It does **not** allow whitespace or hidden comments between `ID`, `"."`, and
`ID`.

### `Rule<T>`

Use `Rule<T>` for parser-level composition.

```cpp
Rule<std::string> QualifiedName{"QualifiedName", some(ID, "."_kw)};
Rule<ast::Entity> EntityRule{
    "Entity",
    "entity"_kw.i() + assign<&ast::Entity::name>(ID)};
```

Rules run with the current skipper, so hidden tokens **can** appear between the
elements of the rule.

That means:

- `Rule<std::string>` becomes a `DataTypeRule`
- `Rule<ast::Entity>` becomes a `ParserRule`

Use `Rule<ValueType>` whenever the construct should tolerate whitespace or
comments between its parts.

## Parser expressions

Pegium provides the usual composition operators for PEG-style grammars:

- sequence with `+`
- choice with `|`
- unordered groups with `&`
- repetition with `option(...)`, `many(...)`, `some(...)`, `repeat<...>(...)`
- positive lookahead with unary `&`
- negative lookahead with unary `!`
- non-greedy range with `<=>`

These operators work both in terminals and rules, but the skipper behavior
differs as described above.

## The basic matching primitives

Three primitives appear in almost every Pegium grammar:

- `Literal`, created with `"_kw"`, for fixed tokens such as keywords,
  punctuation, and operators
- `CharacterRange`, created with `"_cr"`, for one-character lexical classes
- `dot`, for “any single character”

Examples:

```cpp
"entity"_kw.i()      // case-insensitive keyword
"{"_kw               // punctuation
"+"_kw | "-"_kw      // fixed operators
"a-zA-Z_"_cr         // identifier start
"0-9"_cr             // digits
"^\n"_cr             // any character except newline
dot                  // any single character
```

Important details:

- literals are case-sensitive by default, and `.i()` makes them
  case-insensitive
- terminals remain contiguous, so no hidden token can appear inside them
- rules use the skipper, so whitespace and hidden comments may appear between
  their elements

## Assignments and actions

Use assignments and actions to build AST nodes directly from the grammar:

- `assign<&T::member>(...)`
- `append<&T::vectorMember>(...)`
- `enable_if<&T::flag>(...)`
- `create<T>()`
- `action<T>()`
- `nest<&T::member>()`

This is how grammar structure becomes typed AST structure.

## Infix rules

Use `Infix<...>` when the language has operator precedence and associativity.

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

An infix rule:

- starts from a base parser rule such as `PrimaryExpression`
- declares operators with `LeftAssociation(...)` or `RightAssociation(...)`
- produces AST nodes by storing the left operand, operator, and right operand
  into the supplied features

The operator declarations are ordered from the **highest precedence** to the
**lowest precedence**.

In the example above:

1. `%` binds first
2. then `^`
3. then `*` and `/`
4. then `+` and `-`

This ordering controls how expressions are grouped in the AST.

For example:

- `2 + 3 * 4` becomes `2 + (3 * 4)`
- `a - b - c` with `LeftAssociation(...)` becomes `(a - b) - c`
- `a ^ b ^ c` with `RightAssociation(...)` would become `a ^ (b ^ c)`

Concretely, `Infix` lets you describe operator languages without manually
writing one rule per precedence level such as `Addition`, `Multiplication`,
`Exponentiation`, and so on.

That is useful when:

- the language has many operators
- the precedence table is easier to read than a deeply layered rule stack
- you want the AST shape for binary expressions to stay uniform

The base rule is also important. `PrimaryExpression` usually contains the
non-infix atoms of the language:

- literals
- identifiers
- grouped expressions
- function calls

So `Infix` effectively says: start from atomic expressions, then repeatedly
build binary expressions according to the declared precedence table.

Use `Infix` instead of manually chaining precedence levels when the language is
operator-heavy. The `arithmetics` example is the best reference.

## Skippers

Use `SkipperBuilder()` to define ignored and hidden terminals:

```cpp
Skipper skipper =
    SkipperBuilder().ignore(WS).hide(ML_COMMENT, SL_COMMENT).build();
```

- `ignore(...)` consumes tokens that should not appear in the CST at all
- `hide(...)` consumes tokens that should still appear in the CST as hidden
  nodes

You can also use local `with_skipper(...)` overrides on expressions and rules.

In practice, the skipper answers two separate questions:

1. which tokens may appear automatically between parser elements
2. whether those tokens are completely ignored or preserved as hidden CST nodes

### `ignore(...)`

Use `ignore(...)` for tokens that are structurally irrelevant after parsing,
typically whitespace.

Ignored tokens:

- are consumed automatically between elements of parser-level rules
- do not appear in the CST
- cannot be targeted later by CST-based features such as formatting or comment
  processing

### `hide(...)`

Use `hide(...)` for tokens that should not affect parsing, but should remain
available in the CST, typically comments.

Hidden tokens:

- are consumed automatically between elements of parser-level rules
- do appear in the CST
- are marked as hidden nodes
- can still be used later by formatting, hover, or source-aware features

### Where the skipper applies

The skipper applies between the elements of:

- `Rule<T>` used as parser rules
- `Rule<T>` used as data-type rules
- other parser-level compositions that run under the current parser context

So with a skipper in place, a rule such as:

```cpp
Rule<std::string> QualifiedName{"QualifiedName", ID + "."_kw + ID};
```

may accept:

- `foo.bar`
- `foo . bar`
- `foo /* comment */ . bar`

depending on the hidden and ignored terminals in the skipper.

### Where the skipper does not apply

The skipper does **not** split the inside of a `Terminal<T>`.

So:

```cpp
Terminal<std::string> QualifiedNameToken{"QualifiedNameToken", ID + "."_kw + ID};
```

still requires contiguous text and does **not** accept:

- `foo . bar`
- `foo /* comment */ . bar`

This is the key difference between a lexical construct modeled as a terminal and
a parser-level construct modeled as a rule.

## Compile-time constraints

Several grammar APIs reject nullable expressions on purpose. This prevents
ambiguous constructs from silently compiling into fragile grammars.

Use the [grammar reference](../reference/grammar-reference.md) for the
enumerated list of parser elements and the exact rule categories.
