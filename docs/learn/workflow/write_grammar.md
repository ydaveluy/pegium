# 3. Write the Grammar

Your Pegium project is now built and you have chosen an example to start from.
The next step is to define the grammar of your language. In Pegium, that means
writing a `PegiumParser` subclass in C++.

The grammar is the most important part of the language definition. It controls:

- which texts are accepted
- which AST nodes get created
- where comments and whitespace are ignored or preserved
- where references, lists, and nested structures appear

## Example

Here is a shortened version of the `domainmodel` parser:

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

  Rule<ast::Feature> FeatureRule{
      "Feature",
      option(enable_if<&ast::Feature::many>("many"_kw.i())) +
          assign<&ast::Feature::name>(ID) + ":"_kw +
          assign<&ast::Feature::type>(QualifiedName)};

  Rule<ast::DomainModel> Domainmodel{
      "Domainmodel",
      some(append<&ast::DomainModel::elements>(AbstractElementRule))};
};
```

## What is happening here?

### Entry rule

`getEntryRule()` returns the root parser rule. Parsing always starts there. In
practice, this is the rule producing the root AST node of the document.

### Skipper

`getSkipper()` defines how whitespace and comments are handled between parser
elements.

In the example above:

- whitespace is ignored
- line and block comments are hidden
- hidden comments still remain available in the CST for formatting and hover

### Terminals

`ID` is a terminal. Terminals are contiguous lexical rules. They are the right
tool for identifiers, numbers, operators, and comments.

### `Rule<T>`

`QualifiedName` is a `Rule<std::string>`, so it behaves like a data-type rule.
`FeatureRule` and `Domainmodel` are `Rule<ast::...>` values, so they behave as
parser rules and build AST nodes.

### Assignments

Assignments such as `assign<&ast::Feature::name>(ID)` and
`append<&ast::DomainModel::elements>(...)` are where the grammar starts shaping
the semantic model of the language.

## Recommended approach

When writing the grammar of a new language, keep the first iteration small:

1. define the hidden tokens and core terminals
2. create one root rule and one or two declaration rules
3. make parsing work before optimizing the rule hierarchy
4. only then introduce precedence, references, or local skipper tricks

## What to expect at the end of this step

By the end of this step, you should be able to parse representative files of
your language and produce the right top-level AST shape.

## Continue with

- [Grammar Reference](../../reference/grammar-reference.md) for the exact DSL
  surface
- [Glossary](../../reference/glossary.md) for the core parser terms
- [4. Shape the AST and CST](generate_ast.md) for the next workflow step
