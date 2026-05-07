# Multiple Languages

This guide is about integrating multiple dependent languages in one Pegium
workspace.

One common reason to split a language is that one file type defines concepts
while another one consumes them. Pegium's `requirements` example shows exactly
that setup with `.req` and `.tst` files sharing one workspace.

## What is the core idea?

In Pegium, multiple languages usually share the same
`pegium::SharedCoreServices`, while each concrete language gets its
own `pegium::CoreServices` instance.

When you also need editor features, layer the LSP variants
`pegium::SharedServices` and `pegium::Services` on top of the same core setup.

That means:

- documents, indexing, and workspace infrastructure stay shared
- each language still chooses its own parser
- each language can install its own validators and reference logic
- LSP features such as formatters can stay in a dedicated LSP layer
- all registered languages become visible to the same service registry

## A living example

Look at `examples/requirements/src/requirements/core/Module.cpp`.

The example builds one service factory for requirements files and another one
for test files:

```cpp
std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = std::make_unique<RequirementsCoreServices>(sharedServices);
  services->languageMetaData.languageId = std::move(languageId);
  pegium::installDefaultCoreServices(*services);
  installRequirementsCoreModule(*services);
  return services;
}
```

The second factory follows the same pattern but swaps parser, extensions,
and validator for the `tests-lang` language.

The LSP-specific formatters live separately in
`examples/requirements/src/requirements/lsp/Module.cpp`, where the same core
setup is reused and the formatter is added on top.

## Registration

Once each language has its own core service object, register all of them in the
shared registry:

```cpp
bool registerRequirementsServices(pegium::SharedCoreServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}
```

This is the important step that makes both languages available to document
loading and indexing. The LSP registration functions follow the same shape, but
use `pegium::SharedServices` and return the LSP-enabled service containers.

## What usually changes when you split a language?

Expect to review these points for each language:

- parser type
- language id
- file extensions
- validation registration
- scoping and reference behavior
- formatter or other LSP providers when you expose editor features
- editor integration such as syntax highlighting or VS Code contributions

Keep the shared services common unless you intentionally want isolated
workspaces.

## When should you not split?

Do not split a language just because the grammar has several entry rules. Keep
one language when the files share the same lifecycle, the same editor behavior,
and the same symbol space.

Create multiple languages when the file types have clearly distinct identities
or when separate parsers and editor behavior make the model easier to maintain.

## Related pages

- [Examples: Requirements](../examples/requirements.md)
- [Scoping](scoping/index.md)
- [Configuration Services](../reference/configuration-services.md)
