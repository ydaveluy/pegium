# 7. Add Formatting and LSP Services

At this point, you already have a language that parses, links, and validates.
The final common step is to turn that language into a good editing experience.

## Typical additions

- formatter rules
- hover content
- completion customization
- rename behavior
- workspace-aware document lifecycle

## Service wiring

In Pegium, those features are installed through explicit services. A real module
setup looks like this:

```cpp
auto services =
    pegium::services::makeDefaultServices(sharedServices, "domain-model");
services->parser =
    std::make_unique<const domainmodel::parser::DomainModelParser>(*services);
services->lsp.renameProvider =
    std::make_unique<lsp::DomainModelRenameProvider>(
        *services, *sharedServices.workspace.indexManager,
        *sharedServices.workspace.documents, qualifiedNameProvider);
services->lsp.formatter =
    std::make_unique<lsp::DomainModelFormatter>(*services);
```

The important part is that these features are added incrementally. You do not
need to replace the whole language-server stack at once.

## Formatting

The formatter is usually the first editor-facing customization worth adding. It
depends on CST-backed selections, so it becomes much easier once the grammar
and AST shape are stable.

For example, `DomainModelFormatter` formats blocks by selecting the braces and
their interior from the CST:

```cpp
const auto openBrace = formatter.keyword("{");
const auto closeBrace = formatter.keyword("}");
formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
```

## Other LSP features

Pegium already installs several defaults, including completion, hover,
definition, references, rename, document symbols, folding ranges, and code
actions. You only need to override the ones whose behavior must become
language-specific.

## What to expect at the end of this step

By the end of this step, your language should feel complete from the point of
view of an editor user, not just from the point of view of a parser author.

## Continue with

- [Formatting](../../recipes/custom-formatter.md)
- [Default LSP Services](../../reference/lsp-services.md)
- [Document Lifecycle](../../reference/document-lifecycle.md)
- [Recipes](../../recipes/index.md) for the advanced customization path
