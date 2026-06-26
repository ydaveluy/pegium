# Multiple Languages

Integrate several dependent languages in one Pegium workspace. A common driver: one file type defines concepts and another consumes them. Pegium's `requirements` example does exactly that, with `.req` and `.tst` files sharing a workspace.

## Core idea

Multiple languages share one `pegium::SharedCoreServices`, while each concrete language gets its own `pegium::CoreServices` instance. When you need editor features, layer the LSP variants `pegium::SharedServices` and `pegium::Services` on top of the same core setup.

This means:

- Documents, indexing, and workspace infrastructure stay shared.
- Each language chooses its own parser.
- Each language installs its own validators and reference logic.
- LSP features such as formatters stay in a dedicated LSP layer.
- All registered languages become visible to the same service registry.

## A living example

See `examples/requirements/src/requirements/core/CoreModule.cpp`. It builds one service factory for requirements files and another for test files:

```cpp
std::unique_ptr<RequirementsCoreServices>
createRequirementsServices(const pegium::SharedCoreServices &sharedServices,
                           std::string languageId) {
  auto services = pegium::makeDefaultCoreServices<RequirementsCoreServices>(
      sharedServices, std::move(languageId));
  installRequirementsCoreModule(*services);
  return services;
}
```

The second factory follows the same pattern but swaps parser, extensions, and validator for the `tests-lang` language.

The LSP-specific formatters live separately in `examples/requirements/src/requirements/lsp/LspModule.cpp`, where the same core setup is reused and the formatter is added on top.

## Registration

Once each language has its own core service object, register them all in the shared registry:

```cpp
bool registerRequirementsServices(pegium::SharedCoreServices &sharedServices) {
  auto services = createRequirementsAndTestsServices(sharedServices);
  sharedServices.serviceRegistry->registerServices(
      std::move(services.requirements));
  sharedServices.serviceRegistry->registerServices(std::move(services.tests));
  return true;
}
```

This step makes both languages available to document loading and indexing. The LSP registration functions share the same shape but use `pegium::SharedServices` and return the LSP-enabled service containers.

## What changes when you split a language

Review these points for each language:

- Parser type
- Language id
- File extensions
- Validation registration
- Scoping and reference behavior
- Formatter or other LSP providers, when you expose editor features
- Editor integration such as syntax highlighting or VS Code contributions

Keep the shared services common unless you want isolated workspaces.

## When not to split

Do not split a language just because the grammar has several entry rules. Keep one language when the files share the same lifecycle, editor behavior, and symbol space.

Split into multiple languages when the file types have clearly distinct identities, or when separate parsers and editor behavior make the model easier to maintain.

## Related pages

- [Examples: Requirements](../examples/requirements.md)
- [Scoping](scoping/index.md)
- [Configuration Services](../reference/configuration-services.md)
