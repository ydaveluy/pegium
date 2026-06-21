# Error Recovery and Partial Results

When a document has syntax errors, Pegium's parser still produces a best-effort AST plus diagnostics instead of failing. That keeps validation, scoping, and editor features working while the user is still typing.

## What recovery gives you

After building a document, three accessors tell you what happened:

```cpp
document->parseSucceeded();  // true only on a full grammar match
document->hasAst();          // true when a (possibly partial) AST root exists
document->parseRecovered();  // true when recovery kicked in or syntax errors were reported
```

When `parseSucceeded()` is false, `hasAst()` is usually still true: the parser recovers around the broken region and produces a partial root node. Indexing, linking, and validation run on that partial tree, so the editor keeps offering names, references, and validation feedback.

## Reading diagnostics

Every problem — syntax, linking, or validation — lands in `document->diagnostics` as a `pegium::Diagnostic`:

```cpp
for (const auto &d : document->diagnostics) {
  // d.severity : Error | Warning | Information | Hint
  // d.message  : human-readable text
  // d.begin, d.end : byte offsets into the source (map them to line/column via
  //                  the document's TextDocument when you need positions)
  if (d.severity == pegium::DiagnosticSeverity::Error) {
    std::cerr << d.message << " @ " << d.begin << "\n";
  }
}
```

For the headless gate, `pegium::cli::has_error_diagnostics(*document)` returns true when any error-severity diagnostic is present (see [Run a Language Headlessly](../learn/consume-the-ast.md)).

## Testing recovery

Recovery is testable like anything else: feed a broken document and assert that you still get a partial AST and the expected diagnostics.

```cpp
auto document = pegium::test::open_and_build_document(
    *shared, pegium::test::make_file_uri("broken.statemachine"), "statemachine",
    "statemachine Light\n"
    "initialState Idle\n"
    "state Idle\n");   // missing the closing 'end'

ASSERT_NE(document, nullptr);
EXPECT_FALSE(document->parseSucceeded());
EXPECT_TRUE(document->hasAst());           // a partial Statemachine still exists
EXPECT_FALSE(document->diagnostics.empty());
```

## Why it matters for editors

Partial results flow through the whole pipeline, so a language server built on Pegium gives useful completion, hover, and validation even on a file that does not yet parse cleanly — the common case while editing. Recovery is on by default; you do not need to opt in.

## Related pages

- [Run a Language Headlessly](../learn/consume-the-ast.md)
- [Validation](validation.md)
- [Test Your Language](../learn/test-your-language.md)
