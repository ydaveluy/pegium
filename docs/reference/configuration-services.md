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

## Overridable services

These are the services you can replace on a language container. Everything marked **installed** is provided by `makeDefaultServices` / `installDefaultCoreServices`; you assign a field only to override it. The `parser` is the one service with no default — you must set it.

Core language services:

| Field | Interface | Default implementation | Status |
| --- | --- | --- | --- |
| `parser` | `Parser` | — | **Required — you set it** |
| `references.nameProvider` | `NameProvider` | `DefaultNameProvider` | Installed, overridable |
| `references.scopeProvider` | `ScopeProvider` | `DefaultScopeProvider` | Installed, overridable |
| `references.scopeComputation` | `ScopeComputation` | `DefaultScopeComputation` | Installed, overridable |
| `references.linker` | `Linker` | `DefaultLinker` | Installed, overridable |
| `references.references` | `References` | `DefaultReferences` | Installed, overridable |
| `validation.validationRegistry` | `ValidationRegistry` | `DefaultValidationRegistry` | Installed, overridable |
| `validation.documentValidator` | `DocumentValidator` | `DefaultDocumentValidator` | Installed, overridable |
| `documentation.commentProvider` | `CommentProvider` | `DefaultCommentProvider` | Installed, overridable |
| `documentation.documentationProvider` | `DocumentationProvider` | `DefaultDocumentationProvider` | Installed, overridable |
| `workspace.astNodeDescriptionProvider` | `AstNodeDescriptionProvider` | `DefaultAstNodeDescriptionProvider` | Installed, overridable |
| `workspace.referenceDescriptionProvider` | `ReferenceDescriptionProvider` | `DefaultReferenceDescriptionProvider` | Installed, overridable |

LSP feature services, all under `services.lsp`. Installed providers use the framework's `Default…` implementation of the same name (for example `DefaultCompletionProvider`), which you can subclass — see [Custom LSP Features](../recipes/custom-lsp-features.md). Optional providers are absent by default, and the corresponding editor capability is advertised only when you set one:

| Field | Interface | Status |
| --- | --- | --- |
| `lsp.completionProvider` | `CompletionProvider` | Installed, overridable |
| `lsp.hoverProvider` | `HoverProvider` | Installed, overridable |
| `lsp.documentSymbolProvider` | `DocumentSymbolProvider` | Installed, overridable |
| `lsp.documentHighlightProvider` | `DocumentHighlightProvider` | Installed, overridable |
| `lsp.foldingRangeProvider` | `FoldingRangeProvider` | Installed, overridable |
| `lsp.definitionProvider` | `DefinitionProvider` | Installed, overridable |
| `lsp.referencesProvider` | `ReferencesProvider` | Installed, overridable |
| `lsp.renameProvider` | `RenameProvider` | Installed, overridable |
| `lsp.codeActionProvider` | `CodeActionProvider` | Installed, overridable |
| `lsp.declarationProvider` | `DeclarationProvider` | Optional |
| `lsp.typeProvider` | `TypeDefinitionProvider` | Optional |
| `lsp.implementationProvider` | `ImplementationProvider` | Optional |
| `lsp.documentLinkProvider` | `DocumentLinkProvider` | Optional |
| `lsp.selectionRangeProvider` | `SelectionRangeProvider` | Optional |
| `lsp.signatureHelp` | `SignatureHelpProvider` | Optional |
| `lsp.codeLensProvider` | `CodeLensProvider` | Optional |
| `lsp.formatter` | `Formatter` | Optional |
| `lsp.inlayHintProvider` | `InlayHintProvider` | Optional |
| `lsp.semanticTokenProvider` | `SemanticTokenProvider` | Optional |
| `lsp.callHierarchyProvider` | `CallHierarchyProvider` | Optional |
| `lsp.typeHierarchyProvider` | `TypeHierarchyProvider` | Optional |

Because `installDefaultCoreServices` only fills empty slots, the order between installing the defaults and assigning your overrides does not matter — an assigned service is always kept. The one rule to remember is that you must supply a parser and a language id.

### When setup is complete

A container is *complete* once every required core service plus the parser is present. Registering an incomplete container — most often a forgotten parser, since there is no default one — throws `pegium::utils::ServiceRegistrationError` at registration time (Pegium has no compile-time completeness check). `makeDefaultServices` supplies all the defaulted services for you, so in practice you only need to set the parser and the language id.

## Adding your own services

Not every piece of custom logic needs to be a service. If logic has no dependency on the rest of the framework, a plain function or helper class is enough.

When the code depends on other Pegium services, add it as a member of your own container. Every Pegium example follows the same three-part shape:

1. **An `…AddedServices` struct** holding the language-specific members.
2. **A `final` container** that inherits both `pegium::Services` (or `pegium::CoreServices` for headless) and the added struct.
3. **A downcast helper** so framework-invoked services — which only ever see a `const pegium::CoreServices &` — can recover the added members.

```cpp
// Services.hpp
#include <pegium/core/services/ServiceAccess.hpp>

struct MyAddedServices {
  std::unique_ptr<MySummaryService> summaryService;
};

struct MyServices final : pegium::Services, MyAddedServices {
  using pegium::Services::Services;
};

// Recover the added services from a base container reference.
[[nodiscard]] inline const MyAddedServices *
asMyAddedServices(const pegium::CoreServices &services) noexcept {
  return pegium::service_cast<MyAddedServices>(services);
}
```

Then build the container and assign the member in your install-module:

```cpp
auto services = pegium::makeDefaultServices<MyServices>(
    sharedServices, "my-language");
services->summaryService = std::make_unique<MySummaryService>(*services);
```

### Reaching an added service from framework-invoked code

This is the part that makes the `…AddedServices` base necessary. A custom service — say a scope computation — is invoked by the framework through a `const pegium::CoreServices &`, whose static type does **not** carry your added members. There are two ways to recover them.

**Preferred — a typed back-reference.** When a custom service needs *its own* language's siblings, inherit `pegium::LanguageServiceMixin<MyAddedServices>` alongside the Pegium base and construct both from the same container. The added services are then reachable directly as `languageServices.…`, with no cast and no null branch (the reference is as lifetime-safe as the Pegium base's own, since containers are non-movable):

```cpp
class MyScopeComputation final
    : public pegium::references::DefaultScopeComputation,
      public pegium::LanguageServiceMixin<MyAddedServices> {
public:
  template <typename Container>
  explicit MyScopeComputation(const Container &services)
      : pegium::references::DefaultScopeComputation(services),
        pegium::LanguageServiceMixin<MyAddedServices>(services) {}

  void use() const { languageServices.summaryService->run(); } // typed, no cast
};
```

The `domainmodel` example uses exactly this to reach its `qualifiedNameProvider`; see [Qualified Names](../recipes/scoping/qualified-names.md).

**For a cross-language probe — `service_cast`.** When code must decide whether an *arbitrary* container belongs to a particular language (rather than reach its own), recover with `pegium::service_cast<MyAddedServices>(base)` and null-check, since a different language's container can flow through the same path:

```cpp
const auto *added = asMyAddedServices(services); // wraps service_cast
if (added == nullptr) {
  return; // not this language's container
}
use(*added->summaryService);
```

Pegium resolves both at runtime — there is no compile-time service typing — so for the probe form the null-check is the documented norm.

> If a service is reached only by *your own* code and never handed back to the framework through a base reference, you can skip the `…AddedServices` base entirely and use a plain member on your derived container.

## Related pages

- [Document Lifecycle](document-lifecycle.md)
- [LSP Services](../build-a-language/lsp-services.md)
- [Multiple Languages](../recipes/multiple-languages.md)
