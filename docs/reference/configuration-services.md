# Configuration Services

Pegium configures language behavior through explicit service objects. There is one shared service container for runtime-wide concerns, and one language service container per registered language.

For most projects you start from a language container (a `struct MyServices : pegium::Services`, see "Adding your own services" below) and populate it through your own install-module functions:

```cpp
auto services = pegium::makeDefaultServices<MyServices>(
    sharedServices, "my-language");

installMyCoreModule(*services); // parser, file extensions, validation, scoping
installMyLspModule(*services);  // formatter and other LSP overrides
```

`makeDefaultServices<MyServices>(...)` gives you a complete baseline inside your own container. Your install-module functions then add the parser and replace only the pieces that are language-specific. The template argument defaults to the base `pegium::Services`; pass your own type so the container can hold language-specific members.

## Shared services

`SharedServices` owns the runtime pieces reused by every language registered in the same process:

- the service registry and AST reflection
- shared workspace services: documents, index manager, document builder, workspace lock, and workspace manager
- shared LSP runtime services: text documents, the language server, the document update handler, and the fuzzy matcher

Put concerns here when they must stay consistent across the whole workspace or the whole language-server process.

## Language-specific services

Each language gets its own `Services` object. It extends the core language services with `services.lsp`, so one container owns both the semantic layer and the editor-facing layer for that language.

Configure here:

- the parser
- name, scope, and linking services
- validation services
- language-level workspace helpers
- LSP providers such as formatter, hover, rename, or completion

## Overriding services

The common style is to keep the default graph and replace individual services in place, inside your install-module functions:

```cpp
auto services = pegium::makeDefaultServices<MyServices>(
    sharedServices, "my-language");

services->parser = std::make_unique<const my::parser::MyParser>(*services);
services->references.scopeProvider =
    std::make_unique<references::MyScopeProvider>(*services);
services->validation.validationRegistry =
    std::make_unique<validation::MyValidationRegistry>(*services);
services->lsp.formatter = std::make_unique<lsp::MyFormatter>(*services);
```

This explicit style is one of Pegium's main architectural choices: the wiring lives in ordinary C++ code, so it is easy to see what the language depends on.

## Adding your own services

Not every piece of custom logic needs to be a service. If logic has no dependency on the rest of the framework, a plain function or helper class is enough.

When the code depends on other Pegium services, derive your own service container from `pegium::Services` and let `makeDefaultServices<...>(...)` return that derived type:

```cpp
struct MyServices : pegium::Services {
  explicit MyServices(const pegium::SharedServices &shared)
      : pegium::Services(shared) {}

  struct {
    std::unique_ptr<MySummaryService> summaryService;
  } app;
};

auto services = pegium::makeDefaultServices<MyServices>(
    sharedServices, "my-language");
services->app.summaryService =
    std::make_unique<MySummaryService>(*services);
```

This keeps application-specific services close to the language container without forcing you to replace the framework defaults.

## Related pages

- [Document Lifecycle](document-lifecycle.md)
- [LSP Services](../build-a-language/lsp-services.md)
- [Multiple Languages](../recipes/multiple-languages.md)
