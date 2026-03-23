# Completion Provider

`pegium::DefaultCompletionProvider` is the generic content assist base in
Pegium. It consumes parser completion traces, builds a small semantic context
around the cursor, then lets you override narrowly-scoped hooks instead of
rewriting the whole provider.

This keeps the default implementation language-agnostic while still making it
easy to customize references, keywords, rule-level proposals, and snippets.

Pegium produces completion items directly from the completion request and does
not expose a separate `completionItem/resolve` dispatch path. Completion
options are therefore only used for fields that affect completion triggering or
client-side behavior such as trigger characters and commit characters.

At the provider boundary, this is represented by `CompletionProviderOptions`,
which currently supports only:

- `triggerCharacters`
- `allCommitCharacters`

Fields such as `resolveProvider` or `completionItem` are intentionally not part
of the provider options contract because the runtime does not honor them.

## Default flow

1. The parser computes reachable `CompletionFeature` values at the cursor.
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
- `feature`: current parser completion feature, always present for the active
  completion alternative

`tokenText` and `prefix` are borrowed views into the current document text for
the duration of the completion request. Provider hooks should treat them as
read-only slices, not as owned strings.

The `feature` can describe a `Keyword`, a `Reference`, or a `Rule`.

## `CompletionValue`

`CompletionValue` is the generic payload produced by hooks before it is turned
into an LSP item.

Useful fields:

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

If you omit `textEdit`, the default provider computes it from
`tokenOffset..offset`.

## Reference completion

Reference completion is driven by `ReferenceInfo`.

`ReferenceInfo` carries the container node, current reference text, and the
originating grammar `Assignment`. Feature name and expected target type are
derived from that assignment. When a concrete `AbstractReference` already
exists in the AST, the default provider reuses its stored assignment directly.

This context works both:

- when a concrete `AbstractReference` already exists in the recovered document
- when the cursor is on an unfinished reference and only the grammar assignment
  metadata is available

The default provider uses `ScopeProvider::visitScopeEntries(...)`, so custom
scoping logic stays reusable for linking and completion.

## Hooks

### `completionFor`

Top-level dispatch hook. Override it only if you want to replace the routing
between `Reference`, `Rule`, and `Keyword` features.

### `completionForReference`

Called for reference features. The default implementation iterates over
`getReferenceCandidates(...)`, then delegates item creation to
`createReferenceCompletionItem(...)`.

### `getReferenceCandidates`

The best low-risk extension point for filtering reference proposals.

The default implementation enumerates all visible entries through the scope
provider and lets the fuzzy matcher filter them against `context.prefix`.

### `createReferenceCompletionItem`

Transforms one `AstNodeDescription` into a `CompletionValue`. Override this to
change labels, details, documentation, or inserted text for references.

### `completionForRule`

Called for parser rule features. The default implementation emits nothing.

This hook is the natural place to add language templates or snippets tied to a
specific rule.

### `completionForKeyword`

Called for keyword features. The default implementation emits the literal as a
keyword item after `filterKeyword(...)`.

### `filterKeyword`

Cheap boolean filter executed before keyword item creation.

### `fillCompletionItem`

Final hook before returning the LSP item. Override it to add metadata that is
independent from the feature source, such as commit characters or custom text
edits.

### `continueCompletion`

Controls whether later parser features should still be processed. Return
`false` when the first matching feature should stop the pipeline.

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

## Recommended customization order

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
