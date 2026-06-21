# Completion Provider

`pegium::DefaultCompletionProvider` is the generic content assist base in Pegium. It consumes parser completion traces, builds a small semantic context around the cursor, then lets you override narrowly-scoped hooks instead of rewriting the whole provider.

The default stays language-agnostic, so you only customize references, keywords, rule-level proposals, and snippets.

## Default flow

1. The parser computes the reachable completion features (`parser::ExpectPath`) at the cursor.
2. The provider builds a `CompletionContext` for each feature.
3. One of the protected hooks emits `CompletionValue` objects.
4. The default provider turns them into LSP `CompletionItem` values.

## `CompletionContext`

`CompletionContext` gives each hook the document state around the cursor:

- `document`: current workspace document
- `params`: original LSP completion request
- `offset`: absolute cursor offset
- `tokenOffset` / `tokenEndOffset`: token bounds used for replacement
- `tokenText`: token under the cursor, if any
- `prefix`: text from token start to the cursor
- `node`: best AST node found near the completion anchor
- `reference`: concrete reference under the cursor when one already exists
- `feature`: the active completion path (`parser::ExpectPath`), always present for the active completion alternative

The `feature` captures what the grammar expects next at the cursor — typically a keyword, a cross-reference, or a sub-rule.

## `CompletionValue`

`CompletionValue` is the generic payload hooks produce before it becomes an LSP item:

- `label`: visible label, and default inserted text
- `newText`: replacement text when it differs from `label`
- `detail`: short right-side description
- `filterText`: alternate text used by fuzzy filtering
- `sortText`: explicit ordering key
- `kind`: explicit LSP item kind
- `documentation`: Markdown documentation payload
- `textEdit`: custom replacement range
- `insertTextFormat`: set to `::lsp::InsertTextFormat::Snippet` for snippets
- `description`: optional indexed symbol used by the default reference path

Omit `textEdit` and the default provider computes it from `tokenOffset..offset`.

## Reference completion

Reference completion is driven by `ReferenceInfo`, and the default provider uses the `ScopeProvider`. Good scoping gives you good completion for free.

## Hooks

### `completionFor`

Top-level dispatch hook. Override it only to replace the routing between `Reference`, `Rule`, and `Keyword` features.

### `completionForReference`

Called for reference features. The default iterates over `getReferenceCandidates(...)`, then delegates item creation to `createReferenceCompletionItem(...)`.

### `getReferenceCandidates`

The best low-risk extension point for filtering reference proposals.

### `createReferenceCompletionItem`

Transforms one `AstNodeDescription` into a `CompletionValue`.

### `completionForRule`

Called for parser rule features. The natural place to add snippets tied to a specific rule.

### `completionForKeyword`

Called for keyword features. The default emits the literal as a keyword item after `filterKeyword(...)`.

### `filterKeyword`

Cheap boolean filter run before keyword item creation.

### `fillCompletionItem`

Final hook before returning the LSP item.

### `continueCompletion`

Controls whether later parser features are still processed.

## Common override patterns

### Filter reference candidates

```cpp
class MyCompletionProvider final : public pegium::DefaultCompletionProvider {
public:
  using DefaultCompletionProvider::DefaultCompletionProvider;

protected:
  std::vector<const pegium::workspace::AstNodeDescription *>
  getReferenceCandidates(
      const pegium::CompletionContext &context,
      const pegium::ReferenceInfo &reference) const override {
    auto candidates =
        DefaultCompletionProvider::getReferenceCandidates(context, reference);
    std::erase_if(candidates, [](const auto *candidate) {
      return candidate->name.starts_with("_");
    });
    return candidates;
  }
};
```

### Add a keyword hook

```cpp
void completionForKeyword(const pegium::CompletionContext &context,
                          const pegium::grammar::Literal &keyword,
                          const pegium::CompletionAcceptor &acceptor) const override {
  if (keyword.getValue() == "entity" && context.prefix.empty()) {
    acceptor(pegium::CompletionValue{
        .label = "entity",
        .detail = "Top-level declaration",
    });
    return;
  }
  DefaultCompletionProvider::completionForKeyword(context, keyword, acceptor);
}
```

### Add a rule snippet

```cpp
void completionForRule(const pegium::CompletionContext &context,
                       const pegium::grammar::AbstractRule &rule,
                       const pegium::CompletionAcceptor &acceptor) const override {
  if (rule.getName() != "Entity") {
    return;
  }

  acceptor(pegium::CompletionValue{
      .label = "entity",
      .newText = "entity ${1:Name} {\\n\\t$0\\n}",
      .detail = "Snippet",
      .insertTextFormat = ::lsp::InsertTextFormat::Snippet,
  });
}
```

## Provider options

`CompletionProviderOptions` currently supports:

- `triggerCharacters`
- `allCommitCharacters`

## Practical advice

Start from the narrowest hook that solves the problem:

1. `getReferenceCandidates(...)` to filter scope results
2. `createReferenceCompletionItem(...)` to reshape reference items
3. `completionForKeyword(...)` or `filterKeyword(...)` for keyword logic
4. `completionForRule(...)` for snippets and templates
5. `completionFor(...)` only when you need to replace the dispatch strategy

## Related pages

- [LSP Services](../build-a-language/lsp-services.md)
- [Custom LSP Features](../recipes/custom-lsp-features.md)
- [Scoping](../recipes/scoping/index.md)
