# Configuration Services

Pegium configures language behavior through explicit service objects. Instead of
hiding language wiring behind a generated container, it uses one shared service
container for runtime-wide concerns and one language service container per
registered language.

For most projects, the entry point is:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
```

`makeDefaultServices(...)` gives you a complete baseline: default core
services, default LSP services, and the `languageId` already assigned. From
there, you usually add your parser and replace only the pieces that are truly
language-specific.

## Shared services

`SharedServices` owns the runtime pieces reused by every language registered in
the same process.

That includes:

- the service registry and AST reflection
- shared workspace services such as documents, index manager, document builder,
  workspace lock, and workspace manager
- shared LSP runtime services such as text documents, the language server, the
  document update handler, and the fuzzy matcher

These services are the right place for concerns that must stay consistent
across the whole workspace or the whole language-server process.

## Language-specific services

Each language gets its own `Services` object. It extends the core language
services with `services.lsp`, so one container owns both the semantic layer and
the editor-facing layer for that language.

This is where you configure:

- the parser
- name, scope, and linking services
- validation services
- language-level workspace helpers
- LSP providers such as formatter, hover, rename, or completion

## Overriding services

The most common customization style is to keep the default graph and replace
individual services in place:

```cpp
auto services = pegium::services::makeDefaultServices(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
services->references.scopeProvider =
    std::make_unique<references::MyScopeProvider>(*services);
services->validation.validationRegistry =
    std::make_unique<validation::MyValidationRegistry>(*services);
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
```

This explicit style is one of Pegium's main architectural choices: the wiring
is visible in ordinary C++ code, so it is easy to understand what the language
actually depends on.

## Adding your own services

Not every piece of custom logic needs to become a service. If some logic has no
dependency on the rest of the framework, a plain function or helper class is
often enough.

When the code really depends on other Pegium services, derive your own service
container from `pegium::Services` and let `makeDefaultServices<...>(...)`
return that derived type:

```cpp
struct MyServices : pegium::Services {
  explicit MyServices(const pegium::SharedServices &shared)
      : pegium::Services(shared) {}

  struct {
    std::unique_ptr<MySummaryService> summaryService;
  } app;
};

auto services = pegium::services::makeDefaultServices<MyServices>(
    sharedServices, "my-language");
services->app.summaryService =
    std::make_unique<MySummaryService>(*services);
```

That pattern keeps application-specific services close to the language service
container without forcing you to replace the framework defaults.

## Related pages

- [Document Lifecycle](document-lifecycle.md)
- [LSP Services](../build-a-language/lsp-services.md)
- [Multiple Languages](../recipes/multiple-languages.md)
