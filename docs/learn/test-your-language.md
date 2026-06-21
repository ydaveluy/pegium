# Test Your Language

Pegium languages are ordinary C++, so you test them with ordinary C++ tests. The framework ships test helpers under `pegium::test::*` that build a real document through the full pipeline (parse → index → scopes → link → validate), so you can assert on the resulting AST and diagnostics. For working examples, see [`examples/statemachine/tests/`](https://github.com/ydaveluy/pegium/tree/main/examples/statemachine/tests).

## A document-level test

This test registers the language, builds a source string, and asserts on both a validation diagnostic and the typed AST:

```cpp
#include <gtest/gtest.h>

#include <pegium/examples/ExampleTestSupport.hpp> // pegium::test::* helpers
#include <statemachine/lsp/Module.hpp>

TEST(MyLanguage, ParsesAndValidates) {
  // 1. Build a shared runtime and register your language on it.
  auto shared = pegium::test::make_empty_shared_services();
  pegium::installDefaultSharedCoreServices(*shared);
  pegium::installDefaultSharedLspServices(*shared);
  pegium::test::initialize_shared_workspace_for_tests(*shared);
  ASSERT_TRUE(statemachine::lsp::registerStatemachineServices(*shared));

  // 2. Open a source string and run the full build pipeline.
  auto document = pegium::test::open_and_build_document(
      *shared, pegium::test::make_file_uri("machine.statemachine"),
      "statemachine",
      "statemachine Light\n"
      "events Start\n"
      "initialState idle\n"
      "state idle end\n");
  ASSERT_NE(document, nullptr);

  // 3a. Assert on diagnostics (parse, linking, and validation problems).
  EXPECT_TRUE(pegium::test::has_diagnostic_message(
      *document, "State name should start with a capital letter"));

  // 3b. Or assert on the typed AST.
  auto *model = pegium::ast_ptr_cast<statemachine::ast::Statemachine>(
      document->parseResult.value);
  ASSERT_NE(model, nullptr);
  EXPECT_EQ(model->name, "Light");
}
```

What the helpers do:

- `make_empty_shared_services()` plus the `installDefaultShared*Services` / `initialize_shared_workspace_for_tests` calls set up a process-wide runtime — the same shared services your `main` would create.
- `registerStatemachineServices(*shared)` is your own language registration, the function from `lsp/Module.cpp`.
- `open_and_build_document(...)` creates the document and runs every build phase, so by the time it returns, diagnostics are populated and references are linked.
- `has_diagnostic_message(document, needle)` is a convenience over `document->diagnostics`; iterate that vector directly to count or inspect ranges.
- `ast_ptr_cast<T>(document->parseResult.value)` returns the typed root node, or `nullptr` if the cast does not apply.

## Asserting on references and counts

The build pipeline links cross-references, so you can assert on them directly and count diagnostics of a given kind:

```cpp
std::size_t duplicates = 0;
for (const auto &diagnostic : document->diagnostics) {
  if (diagnostic.message.find("Duplicate identifier name:") != std::string::npos) {
    ++duplicates;
  }
}
EXPECT_EQ(duplicates, 4u);
```

## Unit-testing a single grammar rule

To check that a rule parses a snippet (no services, no workspace), declare a standalone rule and pass it to `pegium::test::parse_rule_result` from `<pegium/core/TestRuleParser.hpp>`:

```cpp
using namespace pegium::parser;

Terminal<std::string> ID{"ID", "a-zA-Z_"_cr + many(w)};
Rule<ast::Transition> TransitionRule{
    "Transition", assign<&ast::Transition::event>(ID) + "=>"_kw +
                      assign<&ast::Transition::state>(ID)};

auto result = pegium::test::parse_rule_result(TransitionRule, "Start => Idle");
ASSERT_TRUE(result.value);
```

This is the fastest way to iterate on grammar shape while you are still designing it. References are not linked in a bare rule parse — use the document-level helper above when you need linking or validation.

## Related pages

- [Build a Language End-to-End](walkthrough.md)
- [Validation](../build-a-language/validation.md)
