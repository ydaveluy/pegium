# Custom Formatter

The formatter API is built for method-pointer registration and CST-backed
selections.

## Pattern

```cpp
on<ast::Entity>(&MyFormatter::formatEntity);
onHidden("ML_COMMENT", &MyFormatter::formatComment);
```

That registration belongs in the formatter constructor.

## Inside a formatting method

```cpp
auto formatter = builder.getNodeFormatter(node);
auto openBrace = formatter.keyword("{");
auto closeBrace = formatter.keyword("}");

formatter.keyword("entity").append(oneSpace);
formatBlock(openBrace, closeBrace, formatter.interior(openBrace, closeBrace));
```

## Typical workflow

1. implement one formatting rule for the root node or top-level declarations
2. normalize the obvious spaces around keywords and separators
3. add block formatting
4. only then add comment rewriting if needed

## Good practices

- prefer exact node registrations over one large fallback method
- use the generic block and list helpers to keep rules consistent
- format comments through `onHidden(...)` when the comment text itself matters
- wire the formatter into `services->lsp.formatter`, otherwise nothing runs
